//
// Created by Ilya Nyrkov on 20.08.25.
//

#include "memoria/StatementExecutor.h"

#include "memoria/Database.h"
#include "memoria/Row.h"
#include "memoria/Schema.h"
#include "memoria/Table.h"

#include <algorithm>
#include <functional>
#include <stdexcept>

namespace memoria {

// ----------------------- small helpers -----------------------

static bool valueTypeMatches(ColumnType t, const RowValue& v) {
    return (t == ColumnType::Int && std::holds_alternative<int64_t>(v)) ||
           (t == ColumnType::Str && std::holds_alternative<std::string>(v));
}

// ----------------------- high-level dispatch -----------------------

std::optional<QueryResult> StatementExecutor::execute(const Statement& st) {
    return std::visit(
        [&](const auto& node) -> std::optional<QueryResult> {
            using T = std::decay_t<decltype(node)>;

            if constexpr (std::is_same_v<T, CreateTable>) {
                execCreateTable(node);
                return std::nullopt;
            } else if constexpr (std::is_same_v<T, Insert>) {
                execInsert(node);
                return std::nullopt;
            } else if constexpr (std::is_same_v<T, Delete>) {
                (void)execDelete(node);
                return std::nullopt;
            } else if constexpr (std::is_same_v<T, Update>) {
                (void)execUpdate(node);
                return std::nullopt;
            } else if constexpr (std::is_same_v<T, Select>) {
                return execSelect(node);
            } else {
                static_assert(!sizeof(T*), "Unhandled Statement alternative");
            }
        },
        st);
}

// ----------------------- exec* methods -----------------------

void StatementExecutor::execCreateTable(const CreateTable& st) const {
    db_.createTable(st.tableName, st.schema);
}

void StatementExecutor::execInsert(const Insert& st) const {
    Table& tbl = db_.getTable(st.tableName);
    const Schema& sch = tbl.getSchema();

    // determine ordering of provided columns (or full schema order)
    const std::vector<std::size_t> order = compileInsertColumnOrder(st, sch);

    // each VALUES tuple -> Row (reordered to schema layout)
    for (const auto& vals : st.rows) {
        Row r = makeRowForInsert(vals, order, sch);
        tbl.insertRow(std::move(r));
    }
}

std::size_t StatementExecutor::execDelete(const Delete& st) const {
    Table& tbl = db_.getTable(st.table);

    if (st.where) {
        const auto pred = compileWhere(*st.where, tbl.getSchema());
        return tbl.deleteWhere(pred); // template method defined in header
    } else {
        const std::size_t n = tbl.rowCount();
        tbl.deleteAllRows();
        return n;
    }
}

std::size_t StatementExecutor::execUpdate(const Update& st) const {
    Table& tbl = db_.getTable(st.table);
    const Schema& sch = tbl.getSchema();

    // map assignments (by name) to (index, value) and validate types
    const auto assigns = compileAssignments(st.set, sch);

    if (st.where) {
        const auto pred = compileWhere(*st.where, sch);
        return tbl.updateWhere(pred, assigns);
    } else {
        // update every row
        auto always = [](const Row&) { return true; };
        return tbl.updateWhere(always, assigns);
    }
}

QueryResult StatementExecutor::execSelect(const Select& st) const {
    const Table& tbl = db_.getTable(st.table);
    const Schema& sch = tbl.getSchema();

    QueryResult out;

    // WHERE predicate
    std::function<bool(const Row&)> pred =
        st.where ? compileWhere(*st.where, sch) : [](const Row&) { return true; };

    if (std::holds_alternative<Select::Star>(st.projection)) {
        // header = all columns
        out.header.reserve(sch.size());
        for (const auto& c : sch.columns())
            out.header.push_back(c.name);

        // rows = full rows
        out.rows = tbl.getRowsWhere(pred); // <-- assign, no push_back
    } else {
        const auto& names = std::get<std::vector<std::string>>(st.projection);
        const auto indices = compileProjection(st.projection, sch);

        out.header = names;
        out.rows = tbl.getColumnRowsWhere(indices, pred); // <-- assign, no push_back
    }

    return out;
}

// ----------------------- helper compilers -----------------------

StatementExecutor::Pred StatementExecutor::compileWhere(const WhereExpr& expr,
                                                        const Schema& schema) const {
    // comparison node
    auto cmp = [&](const Comparison& c) -> Pred {
        const std::size_t idx = schema.require_index(c.column);
        const ColumnType t = schema.columns().at(idx).type;

        if (t == ColumnType::Int) {
            if (!std::holds_alternative<int64_t>(c.literal))
                throw std::invalid_argument("WHERE type mismatch: expected int literal");

            const int64_t rhs = std::get<int64_t>(c.literal);
            switch (c.op) {
            case CompareOp::Eq:
                return [=](const Row& r) { return std::get<int64_t>(r.at(idx)) == rhs; };
            case CompareOp::Neq:
                return [=](const Row& r) { return std::get<int64_t>(r.at(idx)) != rhs; };
            case CompareOp::Lt:
                return [=](const Row& r) { return std::get<int64_t>(r.at(idx)) < rhs; };
            case CompareOp::Gt:
                return [=](const Row& r) { return std::get<int64_t>(r.at(idx)) > rhs; };
            case CompareOp::Le:
                return [=](const Row& r) { return std::get<int64_t>(r.at(idx)) <= rhs; };
            case CompareOp::Ge:
                return [=](const Row& r) { return std::get<int64_t>(r.at(idx)) >= rhs; };
            }
        } else { // string column
            if (!std::holds_alternative<std::string>(c.literal))
                throw std::invalid_argument("WHERE type mismatch: expected string literal");

            const std::string rhs = std::get<std::string>(c.literal);
            switch (c.op) {
            case CompareOp::Eq:
                return [=](const Row& r) { return std::get<std::string>(r.at(idx)) == rhs; };
            case CompareOp::Neq:
                return [=](const Row& r) { return std::get<std::string>(r.at(idx)) != rhs; };
            default:
                throw std::invalid_argument("String WHERE supports only = / !=");
            }
        }

        throw std::logic_error("Unhandled CompareOp");
    };

    // recursion
    return std::visit(
        [&](const auto& node) -> Pred {
            using T = std::decay_t<decltype(node)>;
            if constexpr (std::is_same_v<T, Comparison>) {
                return cmp(node);
            } else if constexpr (std::is_same_v<T, And>) {
                Pred L = compileWhere(*node.lhs, schema);
                Pred R = compileWhere(*node.rhs, schema);
                return [L = std::move(L), R = std::move(R)](const Row& r) { return L(r) && R(r); };
            } else if constexpr (std::is_same_v<T, Or>) {
                Pred L = compileWhere(*node.lhs, schema);
                Pred R = compileWhere(*node.rhs, schema);
                return [L = std::move(L), R = std::move(R)](const Row& r) { return L(r) || R(r); };
            } else {
                static_assert(!sizeof(T*), "Unknown WhereExpr alternative");
            }
        },
        expr);
}

std::vector<std::size_t> StatementExecutor::compileProjection(const Select::Projection& proj,
                                                              const Schema& schema) const {
    if (std::holds_alternative<Select::Star>(proj)) {
        std::vector<std::size_t> idx(schema.size());
        for (std::size_t i = 0; i < schema.size(); ++i)
            idx[i] = i;
        return idx;
    }
    const auto& names = std::get<std::vector<std::string>>(proj);
    std::vector<std::size_t> idx;
    idx.reserve(names.size());
    for (const auto& n : names) {
        idx.push_back(schema.require_index(n));
    }
    return idx;
}

std::vector<std::pair<std::size_t, RowValue>>
StatementExecutor::compileAssignments(const std::vector<Assignment>& sets,
                                      const Schema& schema) const {
    std::vector<std::pair<std::size_t, RowValue>> out;
    out.reserve(sets.size());
    for (const auto& a : sets) {
        const std::size_t idx = schema.require_index(a.column);
        const ColumnType t = schema.columns().at(idx).type;
        if (!valueTypeMatches(t, a.value))
            throw std::invalid_argument("UPDATE type mismatch for column '" + a.column + "'");
        out.emplace_back(idx, a.value); // RowValue is copyable here
    }
    return out;
}

std::vector<std::size_t> StatementExecutor::compileInsertColumnOrder(const Insert& st,
                                                                     const Schema& schema) const {
    std::vector<std::size_t> order;

    if (st.columnNames.empty()) {
        order.resize(schema.size());
        for (std::size_t i = 0; i < schema.size(); ++i)
            order[i] = i;
        return order;
    }

    order.reserve(st.columnNames.size());
    for (const auto& name : st.columnNames) {
        order.push_back(schema.require_index(name));
    }
    return order;
}

Row StatementExecutor::makeRowForInsert(const std::vector<RowValue>& values,
                                        const std::vector<std::size_t>& columnOrder,
                                        const Schema& schema) const {
    if (values.size() != columnOrder.size())
        throw std::invalid_argument("INSERT arity mismatch");

    // start with defaults
    std::vector<RowValue> cells;
    cells.reserve(schema.size());
    for (std::size_t i = 0; i < schema.size(); ++i) {
        cells.push_back(schema.default_value(i));
    }

    // plug provided values into schema positions
    for (std::size_t j = 0; j < columnOrder.size(); ++j) {
        const std::size_t idx = columnOrder[j];
        const ColumnType t = schema.columns().at(idx).type;
        const RowValue& v = values[j];

        if (!valueTypeMatches(t, v))
            throw std::invalid_argument("INSERT type mismatch");

        cells[idx] = v; // copy RowValue into the right slot
    }

    return Row{std::move(cells)};
}

} // namespace memoria
