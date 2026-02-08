//
// Created by Ilya Nyrkov on 23.08.25.
//

// tests/executor_test.cpp
#include <gtest/gtest.h>
#include <memoria/Database.h>
#include <memoria/Row.h>
#include <memoria/Schema.h>
#include <memoria/Statement.h>
#include <memoria/StatementExecutor.h>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

using namespace memoria;

// ---------- Helpers: schema, rows, WHERE builders ----------

static Schema schemaStrInt() {
    std::vector<Column> cols;
    cols.push_back(Column{"c1", ColumnType::Str});
    cols.push_back(Column{"c2", ColumnType::Int});
    return Schema{std::move(cols)};
}

static void initTable(Database& db, std::string name = "t") {
    db.createTable(name, schemaStrInt());
}

static RowValue VStr(std::string s) {
    return RowValue{std::move(s)};
}
static RowValue VInt(int64_t v) {
    return RowValue{v};
}

static WhereExpr W(Comparison c) {
    return WhereExpr{std::move(c)};
}
static Comparison Cmp(std::string col, CompareOp op, RowValue lit) {
    return Comparison{std::move(col), op, std::move(lit)};
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

// convenience: SELECT * FROM table [WHERE ...]
static Select selectStar(std::string table, std::optional<WhereExpr> w = std::nullopt) {
    Select s;
    s.table = std::move(table);
    s.projection = Select::Star{};
    s.where = std::move(w);
    return s;
}

// convenience: SELECT c1,c2 FROM table [WHERE ...]
static Select selectCols(std::string table, std::vector<std::string> cols,
                         std::optional<WhereExpr> w = std::nullopt) {
    Select s;
    s.table = std::move(table);
    s.projection = std::move(cols);
    s.where = std::move(w);
    return s;
}

// convenience: INSERT INTO t [(cols...)] VALUES rows...
static Insert insertRows(std::string table, std::vector<std::string> cols,
                         std::vector<std::vector<RowValue>> rows) {
    Insert ins;
    ins.tableName = std::move(table);
    ins.columnNames = std::move(cols);
    ins.rows = std::move(rows);
    return ins;
}

// convenience: UPDATE t SET (name,value)...
static Update updateSet(std::string table, std::vector<Assignment> sets,
                        std::optional<WhereExpr> where = std::nullopt) {
    Update u;
    u.table = std::move(table);
    u.set = std::move(sets);
    u.where = std::move(where);
    return u;
}

// convenience: DELETE FROM t [WHERE ...]
static Delete deleteFrom(std::string table, std::optional<WhereExpr> w = std::nullopt) {
    Delete d;
    d.table = std::move(table);
    d.where = std::move(w);
    return d;
}

// ---- tiny check helpers ----
static std::string asStr(const Row& r, size_t i) {
    return std::get<std::string>(r.at(i));
}
static int64_t asInt(const Row& r, size_t i) {
    return std::get<int64_t>(r.at(i));
}

// ============================================================
//                       Tests
// ============================================================

TEST(StatementExecutor, ExecCreateTable_CreatesAndRejectsDuplicate) {
    Database db;
    StatementExecutor exec{db};

    // create ok
    exec.execCreateTable(CreateTable{"t", schemaStrInt()});
    EXPECT_TRUE(db.hasTable("t"));

    // duplicate rejects
    try {
        exec.execCreateTable(CreateTable{"t", schemaStrInt()});
        FAIL() << "Expected invalid_argument for duplicate table";
    } catch (const std::invalid_argument&) {
        SUCCEED();
    } catch (...) {
        FAIL() << "Unexpected exception type";
    }
}

TEST(StatementExecutor, ExecInsert_Basic_NoColumns) {
    Database db;
    initTable(db);
    StatementExecutor exec{db};

    // INSERT INTO t VALUES ('a', 1)
    exec.execInsert(insertRows("t", {}, {{VStr("a"), VInt(1)}}));

    // SELECT * to verify
    QueryResult qr = exec.execSelect(selectStar("t"));
    ASSERT_EQ(qr.rows.size(), 1u);
    EXPECT_EQ(asStr(qr.rows[0], 0), "a");
    EXPECT_EQ(asInt(qr.rows[0], 1), 1);
}

TEST(StatementExecutor, ExecInsert_WithColumns_Reorders) {
    Database db;
    initTable(db);
    StatementExecutor exec{db};

    // INSERT INTO t (c2,c1) VALUES (5,'b')
    exec.execInsert(insertRows("t", {"c2", "c1"}, {{VInt(5), VStr("b")}}));

    QueryResult qr = exec.execSelect(selectStar("t"));
    ASSERT_EQ(qr.rows.size(), 1u);
    EXPECT_EQ(asStr(qr.rows[0], 0), "b"); // reordered to schema order (c1,c2)
    EXPECT_EQ(asInt(qr.rows[0], 1), 5);
}

TEST(StatementExecutor, ExecInsert_MultiRow) {
    Database db;
    initTable(db);
    StatementExecutor exec{db};

    exec.execInsert(insertRows("t", {}, {{VStr("a"), VInt(1)}, {VStr("b"), VInt(2)}}));

    QueryResult qr = exec.execSelect(selectStar("t"));
    ASSERT_EQ(qr.rows.size(), 2u);
    EXPECT_EQ(asStr(qr.rows[0], 0), "a");
    EXPECT_EQ(asInt(qr.rows[0], 1), 1);
    EXPECT_EQ(asStr(qr.rows[1], 0), "b");
    EXPECT_EQ(asInt(qr.rows[1], 1), 2);
}

TEST(StatementExecutor, ExecInsert_ArityOrTypeMismatchThrows) {
    Database db;
    initTable(db);
    StatementExecutor exec{db};

    // wrong arity (only one value)
    try {
        exec.execInsert(insertRows("t", {}, {{VStr("x")}}));
        FAIL() << "Expected invalid_argument for arity mismatch";
    } catch (const std::invalid_argument&) {
        SUCCEED();
    } catch (...) {
        FAIL() << "Unexpected exception type";
    }

    // wrong type order: (int,str) vs (str,int)
    try {
        exec.execInsert(insertRows("t", {}, {{VInt(7), VStr("x")}}));
        FAIL() << "Expected invalid_argument for type mismatch";
    } catch (const std::invalid_argument&) {
        SUCCEED();
    } catch (...) {
        FAIL() << "Unexpected exception type";
    }
}

TEST(StatementExecutor, ExecDelete_WithWhere) {
    Database db;
    initTable(db);
    StatementExecutor exec{db};

    exec.execInsert(insertRows("t", {}, {{VStr("a"), VInt(1)}, {VStr("b"), VInt(2)}}));

    // DELETE FROM t WHERE c2 >= 2
    size_t removed = exec.execDelete(deleteFrom("t", W(Cmp("c2", CompareOp::Ge, VInt(2)))));
    EXPECT_EQ(removed, 1u);

    QueryResult qr = exec.execSelect(selectStar("t"));
    ASSERT_EQ(qr.rows.size(), 1u);
    EXPECT_EQ(asStr(qr.rows[0], 0), "a");
}

TEST(StatementExecutor, ExecDelete_NoWhereDeletesAll) {
    Database db;
    initTable(db);
    StatementExecutor exec{db};
    exec.execInsert(insertRows("t", {}, {{VStr("a"), VInt(1)}, {VStr("b"), VInt(2)}}));

    // std::nullopt WHERE -> delete all
    size_t removed = exec.execDelete(deleteFrom("t", std::nullopt));
    EXPECT_EQ(removed, 2u);

    QueryResult qr = exec.execSelect(selectStar("t"));
    EXPECT_TRUE(qr.rows.empty());
}

TEST(StatementExecutor, ExecUpdate_SetWhere) {
    Database db;
    initTable(db);
    StatementExecutor exec{db};
    exec.execInsert(insertRows("t", {}, {{VStr("a"), VInt(1)}, {VStr("b"), VInt(2)}}));

    // UPDATE t SET c2 = 7 WHERE c1 = 'a'
    std::vector<Assignment> sets = {Assignment{"c2", VInt(7)}};
    size_t changed = exec.execUpdate(updateSet("t", sets, W(Cmp("c1", CompareOp::Eq, VStr("a")))));
    EXPECT_EQ(changed, 1u);

    QueryResult qr = exec.execSelect(selectStar("t"));
    ASSERT_EQ(qr.rows.size(), 2u);
    // rows order preserved; first row had c1='a'
    EXPECT_EQ(asInt(qr.rows[0], 1), 7);
}

TEST(StatementExecutor, ExecUpdate_UnknownColumnOrTypeMismatchThrows) {
    Database db;
    initTable(db);
    StatementExecutor exec{db};
    exec.execInsert(insertRows("t", {}, {{VStr("a"), VInt(1)}}));

    // unknown column
    try {
        exec.execUpdate(updateSet("t", {Assignment{"nope", VInt(5)}}));
        FAIL() << "Expected out_of_range for unknown column";
    } catch (const std::out_of_range&) {
        SUCCEED();
    } catch (...) {
        FAIL() << "Unexpected exception type";
    }

    // type mismatch: set c2 (int) to string
    try {
        exec.execUpdate(updateSet("t", {Assignment{"c2", VStr("oops")}}));
        FAIL() << "Expected invalid_argument for type mismatch";
    } catch (const std::invalid_argument&) {
        SUCCEED();
    } catch (...) {
        FAIL() << "Unexpected exception type";
    }
}

TEST(StatementExecutor, ExecSelect_ProjectionAndWhere) {
    Database db;
    initTable(db);
    StatementExecutor exec{db};
    exec.execInsert(insertRows("t", {}, {{VStr("a"), VInt(1)}, {VStr("b"), VInt(2)}}));

    // SELECT c1 FROM t WHERE c2 > 1
    Select sel = selectCols("t", {"c1"}, W(Cmp("c2", CompareOp::Gt, VInt(1))));
    QueryResult qr = exec.execSelect(sel);

    ASSERT_EQ(qr.header.size(), 1u);
    EXPECT_EQ(qr.header[0], "c1");
    ASSERT_EQ(qr.rows.size(), 1u);
    EXPECT_EQ(asStr(qr.rows[0], 0), "b");
}

TEST(StatementExecutor, ExecSelect_UnknownProjectionThrows) {
    Database db;
    initTable(db);
    StatementExecutor exec{db};
    exec.execInsert(insertRows("t", {}, {{VStr("a"), VInt(1)}}));

    try {
        (void)exec.execSelect(selectCols("t", {"nope"}));
        FAIL() << "Expected out_of_range for unknown column projection";
    } catch (const std::out_of_range&) {
        SUCCEED();
    } catch (...) {
        FAIL() << "Unexpected exception type";
    }
}

TEST(StatementExecutor, ExecSelect_WhereTypeMismatchThrows) {
    Database db;
    initTable(db);
    StatementExecutor exec{db};
    exec.execInsert(insertRows("t", {}, {{VStr("a"), VInt(1)}}));

    // WHERE c1 (string) >= 2 -> invalid
    try {
        (void)exec.execSelect(selectStar("t", W(Cmp("c1", CompareOp::Ge, VInt(2)))));
        FAIL() << "Expected invalid_argument for WHERE type mismatch";
    } catch (const std::invalid_argument&) {
        SUCCEED();
    } catch (...) {
        FAIL() << "Unexpected exception type";
    }
}

TEST(StatementExecutor, Execute_Dispatch) {
    Database db;
    StatementExecutor exec{db};

    // CREATE -> nullopt
    Statement s1 = CreateTable{"t", schemaStrInt()};
    auto r1 = exec.execute(s1);
    EXPECT_FALSE(r1.has_value());
    EXPECT_TRUE(db.hasTable("t"));

    // INSERT -> nullopt
    Statement s2 = insertRows("t", {}, {{VStr("x"), VInt(10)}});
    auto r2 = exec.execute(s2);
    EXPECT_FALSE(r2.has_value());

    // SELECT -> result
    Statement s3 = selectStar("t");
    auto r3 = exec.execute(s3);
    ASSERT_TRUE(r3.has_value());
    ASSERT_EQ(r3->rows.size(), 1u);
    EXPECT_EQ(asStr(r3->rows[0], 0), "x");
    EXPECT_EQ(asInt(r3->rows[0], 1), 10);

    // DELETE -> count via execDelete; execute returns nullopt
    Statement s4 = deleteFrom("t", std::nullopt);
    auto r4 = exec.execute(s4);
    EXPECT_FALSE(r4.has_value());
    QueryResult after = exec.execSelect(selectStar("t"));
    EXPECT_TRUE(after.rows.empty());
}
