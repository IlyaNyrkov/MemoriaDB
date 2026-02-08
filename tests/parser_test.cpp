//
// Created by Ilya Nyrkov on 22.08.25.
//
// tests/parser_test.cpp
#include <gtest/gtest.h>
#include <memoria/Parser.h>
#include <memoria/Row.h>
#include <memoria/Schema.h>
#include <memoria/Statement.h>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

// --------- Helpers to build WHERE AST & compare deeply ---------

using namespace memoria;

static Comparison Cmp(std::string col, CompareOp op, RowValue lit) {
    return Comparison{std::move(col), op, std::move(lit)};
}

static WhereExpr W(Comparison c) {
    return WhereExpr{std::move(c)};
}

static WhereExpr WAnd(WhereExpr l, WhereExpr r) {
    And a;
    a.lhs = std::make_unique<WhereExpr>(std::move(l));
    a.rhs = std::make_unique<WhereExpr>(std::move(r));
    return WhereExpr{std::move(a)};
}

static WhereExpr WOr(WhereExpr l, WhereExpr r) {
    Or o;
    o.lhs = std::make_unique<WhereExpr>(std::move(l));
    o.rhs = std::make_unique<WhereExpr>(std::move(r));
    return WhereExpr{std::move(o)};
}

static bool rowValueEq(const RowValue& a, const RowValue& b) {
    if (a.index() != b.index())
        return false;
    if (std::holds_alternative<int64_t>(a))
        return std::get<int64_t>(a) == std::get<int64_t>(b);
    return std::get<std::string>(a) == std::get<std::string>(b);
}

// compare to WhereExpr objects
static bool whereEq(const WhereExpr& A, const WhereExpr& B) {
    return std::visit(
        [&](auto const& x) -> bool {
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<T, Comparison>) {
                if (!std::holds_alternative<Comparison>(B))
                    return false;
                const auto& y = std::get<Comparison>(B);
                return x.column == y.column && x.op == y.op && rowValueEq(x.literal, y.literal);
            } else if constexpr (std::is_same_v<T, And>) {
                if (!std::holds_alternative<And>(B))
                    return false;
                const auto& y = std::get<And>(B);
                return whereEq(*x.lhs, *y.lhs) && whereEq(*x.rhs, *y.rhs);
            } else if constexpr (std::is_same_v<T, Or>) {
                if (!std::holds_alternative<Or>(B))
                    return false;
                const auto& y = std::get<Or>(B);
                return whereEq(*x.lhs, *y.lhs) && whereEq(*x.rhs, *y.rhs);
            } else {
                return false;
            }
        },
        A);
}

static void expectWhereEq(const std::optional<WhereExpr>& got,
                          const std::optional<WhereExpr>& exp) {
    if (!exp.has_value()) {
        EXPECT_FALSE(got.has_value());
        return;
    }
    ASSERT_TRUE(got.has_value());
    EXPECT_TRUE(whereEq(*got, *exp));
}

// --------- Tests ---------

TEST(Parser, CreateTable_Basic) {
    Parser p;
    auto st = p.prepareStatement("CREATE TABLE t (c1 INT, c2 STR);");

    ASSERT_TRUE(std::holds_alternative<CreateTable>(st));
    const auto& ct = std::get<CreateTable>(st);
    EXPECT_EQ(ct.tableName, "t");
    ASSERT_EQ(ct.schema.size(), 2u);
    EXPECT_EQ(ct.schema.columns()[0].name, "c1");
    EXPECT_EQ(ct.schema.columns()[0].type, ColumnType::Int);
    EXPECT_EQ(ct.schema.columns()[1].name, "c2");
    EXPECT_EQ(ct.schema.columns()[1].type, ColumnType::Str);
}

TEST(Parser, Insert_Values) {
    Parser p;
    auto st = p.prepareStatement("INSERT INTO t VALUES (42, 'foo');");

    ASSERT_TRUE(std::holds_alternative<Insert>(st));
    const auto& ins = std::get<Insert>(st);

    EXPECT_EQ(ins.tableName, "t");
    EXPECT_TRUE(ins.columnNames.empty()); // no column list provided

    ASSERT_EQ(ins.rows.size(), 1u); // one VALUES tuple
    const auto& row0 = ins.rows[0];
    ASSERT_EQ(row0.size(), 2u);

    EXPECT_TRUE(std::holds_alternative<int64_t>(row0[0]));
    EXPECT_EQ(std::get<int64_t>(row0[0]), 42);

    EXPECT_TRUE(std::holds_alternative<std::string>(row0[1]));
    EXPECT_EQ(std::get<std::string>(row0[1]), "foo");
}

TEST(Parser, Insert_WithColumnList) {
    Parser p;
    auto st = p.prepareStatement("INSERT INTO t (c2, c1) VALUES (7, 'x');");

    ASSERT_TRUE(std::holds_alternative<Insert>(st));
    const auto& ins = std::get<Insert>(st);

    EXPECT_EQ(ins.tableName, "t");
    ASSERT_EQ(ins.columnNames.size(), 2u);
    EXPECT_EQ(ins.columnNames[0], "c2");
    EXPECT_EQ(ins.columnNames[1], "c1");

    ASSERT_EQ(ins.rows.size(), 1u);
    const auto& row0 = ins.rows[0];
    ASSERT_EQ(row0.size(), 2u);
    EXPECT_EQ(std::get<int64_t>(row0[0]), 7);
    EXPECT_EQ(std::get<std::string>(row0[1]), "x");
}

TEST(Parser, Insert_MultiRow) {
    Parser p;
    auto st = p.prepareStatement("INSERT INTO t VALUES (1,'a'), (2,'b'), (3,'c');");

    ASSERT_TRUE(std::holds_alternative<Insert>(st));
    const auto& ins = std::get<Insert>(st);

    EXPECT_EQ(ins.tableName, "t");
    EXPECT_TRUE(ins.columnNames.empty());

    ASSERT_EQ(ins.rows.size(), 3u);

    EXPECT_EQ(std::get<int64_t>(ins.rows[0][0]), 1);
    EXPECT_EQ(std::get<std::string>(ins.rows[0][1]), "a");

    EXPECT_EQ(std::get<int64_t>(ins.rows[1][0]), 2);
    EXPECT_EQ(std::get<std::string>(ins.rows[1][1]), "b");

    EXPECT_EQ(std::get<int64_t>(ins.rows[2][0]), 3);
    EXPECT_EQ(std::get<std::string>(ins.rows[2][1]), "c");
}

TEST(Parser, Delete_WithWhere) {
    Parser p;
    Statement st = p.prepareStatement("DELETE FROM t WHERE c1 = 10;");

    ASSERT_TRUE(std::holds_alternative<Delete>(st));
    const auto& del = std::get<Delete>(st);
    EXPECT_EQ(del.table, "t");

    auto expected = std::optional<WhereExpr>{W(Cmp("c1", CompareOp::Eq, RowValue{int64_t{10}}))};

    expectWhereEq(del.where, expected);
}

TEST(Parser, Update_Set_WithWhere_AND) {
    Parser p;
    auto st = p.prepareStatement("UPDATE t SET c2 = 7, c1 = 'x' WHERE c2 >= 3 AND c1 != 'y';");

    ASSERT_TRUE(std::holds_alternative<Update>(st));
    const auto& up = std::get<Update>(st);
    EXPECT_EQ(up.table, "t");
    ASSERT_EQ(up.set.size(), 2u);
    EXPECT_EQ(up.set[0].column, "c2");
    EXPECT_TRUE(std::holds_alternative<int64_t>(up.set[0].value));
    EXPECT_EQ(std::get<int64_t>(up.set[0].value), 7);
    EXPECT_EQ(up.set[1].column, "c1");
    EXPECT_TRUE(std::holds_alternative<std::string>(up.set[1].value));
    EXPECT_EQ(std::get<std::string>(up.set[1].value), "x");

    WhereExpr expWhere = WAnd(W(Cmp("c2", CompareOp::Ge, RowValue{int64_t{3}})),
                              W(Cmp("c1", CompareOp::Neq, RowValue{std::string{"y"}})));

    std::optional<WhereExpr> expected;
    expected.emplace(std::move(expWhere));

    const auto& upd = std::get<Update>(st);

    expectWhereEq(upd.where, expected);
}

TEST(Parser, Select_Star_WithWhere_OR) {
    Parser p;
    auto st = p.prepareStatement("SELECT * FROM t WHERE c2 < 5 OR c1 = 'hi';");

    ASSERT_TRUE(std::holds_alternative<Select>(st));
    const auto& sel = std::get<Select>(st);
    EXPECT_EQ(sel.table, "t");
    ASSERT_TRUE(std::holds_alternative<Select::Star>(sel.projection));

    // expect WHERE to be attached inside Select
    WhereExpr expWhere = WOr(W(Cmp("c2", CompareOp::Lt, RowValue{int64_t{5}})),
                             W(Cmp("c1", CompareOp::Eq, RowValue{std::string{"hi"}})));
    std::optional<WhereExpr> expected;
    expected.emplace(std::move(expWhere));

    ASSERT_TRUE(sel.where.has_value());
    expectWhereEq(sel.where, expected);
}

TEST(Parser, Select_Columns_NoWhere) {
    Parser p;
    auto st = p.prepareStatement("SELECT c1, c2 FROM t;");

    ASSERT_TRUE(std::holds_alternative<Select>(st));
    const auto& sel = std::get<Select>(st);
    EXPECT_EQ(sel.table, "t");
    ASSERT_TRUE(std::holds_alternative<std::vector<std::string>>(sel.projection));
    const auto& cols = std::get<std::vector<std::string>>(sel.projection);
    ASSERT_EQ(cols.size(), 2u);
    EXPECT_EQ(cols[0], "c1");
    EXPECT_EQ(cols[1], "c2");
}

TEST(Parser, Script_SplitOutsideQuotes) {
    Parser p;
    std::string script = "INSERT INTO t VALUES (1, 'a;b;c');"
                         "DELETE FROM t WHERE c1 = 1;  "
                         "SELECT * FROM t;";

    auto parsed = p.prepareStatements(script);
    ASSERT_EQ(parsed.size(), 3u);

    // #1 INSERT
    {
        const auto& st = parsed[0];
        ASSERT_TRUE(std::holds_alternative<Insert>(st));
        const auto& ins = std::get<Insert>(st);
        EXPECT_EQ(ins.tableName, "t");
        EXPECT_TRUE(ins.columnNames.empty());

        ASSERT_EQ(ins.rows.size(), 1u); // one VALUES tuple
        const auto& row0 = ins.rows[0];
        ASSERT_EQ(row0.size(), 2u);

        EXPECT_TRUE(std::holds_alternative<int64_t>(row0[0]));
        EXPECT_EQ(std::get<int64_t>(row0[0]), 1);

        EXPECT_TRUE(std::holds_alternative<std::string>(row0[1]));
        EXPECT_EQ(std::get<std::string>(row0[1]), "a;b;c");
    }
    // #2 DELETE WHERE
    {
        const auto& st = parsed[1];
        ASSERT_TRUE(std::holds_alternative<Delete>(st));
        auto exp = std::optional<WhereExpr>{W(Cmp("c1", CompareOp::Eq, RowValue{int64_t{1}}))};

        const auto& del = std::get<Delete>(st);

        expectWhereEq(del.where, exp);
    }
    // #3 SELECT *
    {
        const auto& st = parsed[2];
        ASSERT_TRUE(std::holds_alternative<Select>(st));
        const auto& sel = std::get<Select>(st);
        ASSERT_TRUE(std::holds_alternative<Select::Star>(sel.projection));
    }
}

TEST(Parser, Whitespace_And_TrailingSemicolonOptional) {
    Parser p;
    auto st1 = p.prepareStatement("  INSERT INTO t VALUES(2,'x')   ");
    auto st2 = p.prepareStatement("INSERT INTO t VALUES(2,'x');");

    ASSERT_TRUE(std::holds_alternative<Insert>(st1));
    ASSERT_TRUE(std::holds_alternative<Insert>(st2));

    const auto& rowsA = std::get<Insert>(st1).rows;
    const auto& rowsB = std::get<Insert>(st2).rows;

    ASSERT_EQ(rowsA.size(), 1u);
    ASSERT_EQ(rowsB.size(), 1u);

    const auto& a = rowsA[0]; // first (and only) row
    const auto& b = rowsB[0];

    ASSERT_EQ(a.size(), 2u);
    ASSERT_EQ(b.size(), 2u);

    EXPECT_EQ(std::get<int64_t>(a[0]), 2);
    EXPECT_EQ(std::get<std::string>(a[1]), "x");

    EXPECT_EQ(std::get<int64_t>(b[0]), 2);
    EXPECT_EQ(std::get<std::string>(b[1]), "x");
}

TEST(Parser, CaseSensitivity_Keywords) {
    Parser p;

    try {
        (void)p.prepareStatement("create table t (c INT)");
        FAIL() << "Expected memoria::ParseError due to lowercase keywords";
    } catch (const memoria::ParseError&) {
        SUCCEED();
    } catch (const std::exception& e) {
        FAIL() << "Expected memoria::ParseError, got " << typeid(e).name()
               << " with what(): " << e.what();
    } catch (...) {
        FAIL() << "Expected memoria::ParseError, got non-std exception";
    }
}