//
// Created by Ilya Nyrkov on 19.08.25.
//

#include <utility>

#include "memoria/Table.h"
namespace memoria {

    static bool valueTypeMatches(ColumnType t, const RowValue& v) {
        return (t == ColumnType::Int && std::holds_alternative<int64_t>(v)) ||
               (t == ColumnType::Str && std::holds_alternative<std::string>(v));
    }

    const Schema& Table::getSchema() const noexcept {
        return schema_;
    }

    std::size_t Table::rowCount() const noexcept {
        return rows_.size();
    }

    void Table::insertRow(Row row) {
        // arity check
        if (row.size() != schema_.size()) {
            throw std::invalid_argument("Row arity mismatch");
        }
        // types
        for (std::size_t i = 0; i < schema_.size(); ++i) {
            const auto& col = schema_.columns().at(i);
            if (!valueTypeMatches(col.type, row.at(i))) {
                throw std::invalid_argument("Row type mismatch at column " + std::to_string(i));
            }
        }
        rows_.push_back(std::move(row));
    }

    void Table::deleteAllRows() {
        rows_.clear();
    }

}