//
// Created by Ilya Nyrkov on 22.08.25.
//

#include <gtest/gtest.h>
#include <memoria/Table.h>
#include <memoria/Schema.h>
#include <memoria/Row.h>
#include "memoria/Predicate.h"

#include <string>
#include <vector>
#include <variant>
#include <cstdint>
#include <stdexcept>

using namespace memoria;

static Schema schemaStrInt() {
    return Schema{{ {"c1", ColumnType::Str}, {"c2", ColumnType::Int} }};
}
static Row rowSI(std::string s, int64_t i) {
    std::vector<RowValue> v;
    v.emplace_back(std::move(s));
    v.emplace_back(std::in_place_type<int64_t>, i);
    return Row{ std::move(v) };
}
static int64_t asInt(const Row& r, size_t i) { return std::get<int64_t>(r.at(i)); }
static std::string asStr(const Row& r, size_t i) { return std::get<std::string>(r.at(i)); }

TEST(Table, InsertAndRowCount) {
    Table t{ schemaStrInt() };
    EXPECT_EQ(t.rowCount(), 0u);
    t.insertRow(rowSI("a", 1));
    t.insertRow(rowSI("b", 2));
    EXPECT_EQ(t.rowCount(), 2u);
}


TEST(Table, InsertRejectsWrongArity) {
    Table t{ schemaStrInt() };
    std::vector<RowValue> v;
    v.emplace_back(std::in_place_type<std::string>, "only");

    try {
        t.insertRow(Row{v});
        FAIL() << "Expected std::invalid_argument";
    } catch (const std::invalid_argument&) {
        SUCCEED();
    } catch (const std::exception& e) {
        FAIL() << "Expected std::invalid_argument, got std::exception: " << e.what();
    } catch (...) {
        FAIL() << "Expected std::invalid_argument, got non-std exception";
    }
}


TEST(Table, InsertRejectsTypeMismatch) {
    Table t{ schemaStrInt() };
    // (int, str) vs schema (str, int)
    std::vector<RowValue> v;
    v.emplace_back(std::in_place_type<int64_t>, 7);
    v.emplace_back(std::in_place_type<std::string>, "x");

    try {
        t.insertRow(Row{v});
        FAIL() << "Expected std::invalid_argument";
    } catch (const std::invalid_argument&) {
        SUCCEED();
    } catch (const std::exception& e) {
        FAIL() << "Expected std::invalid_argument, got std::exception: " << e.what();
    } catch (...) {
        FAIL() << "Expected std::invalid_argument, got non-std exception";
    }
}

TEST(Table, SelectWhere_FullRows) {
    Table t{ schemaStrInt() };
    t.insertRow(rowSI("a", 1));
    t.insertRow(rowSI("b", 2));
    t.insertRow(rowSI("b", 3));

    auto rows = t.getRowsWhere([](const Row& r) {
        return std::get<std::string>(r.at(0)) == "b";
    });
    ASSERT_EQ(rows.size(), 2u);
    EXPECT_EQ(asStr(rows.at(0), 0), "b");
    EXPECT_EQ(asInt(rows.at(0), 1), 2);
    EXPECT_EQ(asStr(rows.at(1), 0), "b");
    EXPECT_EQ(asInt(rows.at(1), 1), 3);
}

TEST(Table, GetRowsWhere_InvalidProjectionIndexThrows) {
    Table t{ schemaStrInt() };
    t.insertRow(rowSI("x", 10));
    std::vector<size_t> bad = {2}; // schema has only 2 columns

    try {
        // Use your current method name; if it's getRowsWhere(), replace below accordingly.
        (void)t.getColumnRowsWhere(bad, [](const Row&){ return true; });
        FAIL() << "Expected std::out_of_range";
    } catch (const std::out_of_range&) {
        SUCCEED();
    } catch (const std::exception& e) {
        FAIL() << "Expected std::out_of_range, got std::exception: " << e.what();
    } catch (...) {
        FAIL() << "Expected std::out_of_range, got non-std exception";
    }
}

TEST(Table, DeleteWhere_RemovesAndCounts) {
    Table t{ schemaStrInt() };
    t.insertRow(rowSI("a", 1));
    t.insertRow(rowSI("b", 2));
    t.insertRow(rowSI("c", 2));
    EXPECT_EQ(t.rowCount(), 3u);

    size_t removed = t.deleteWhere([](const Row& r){ return asInt(r,1) == 2; });
    EXPECT_EQ(removed, 2u);
    EXPECT_EQ(t.rowCount(), 1u);

    auto rows = t.getRowsWhere([](const Row&){ return true; });
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(asStr(rows.at(0),0), "a");
    EXPECT_EQ(asInt(rows.at(0),1), 1);
}

TEST(Table, UpdateWhere_Assignments) {
    Table t{ schemaStrInt() };
    t.insertRow(rowSI("old", 1));
    t.insertRow(rowSI("x", 2));
    t.insertRow(rowSI("y", 3));

    // set c1 = "new" where c2 < 3
    std::vector<std::pair<size_t, RowValue>> assigns;

    assigns.emplace_back(0, RowValue{std::in_place_type<std::string>, "new"});

    size_t updated = t.updateWhere([](const Row& r){ return asInt(r,1) < 3; }, assigns);
    EXPECT_EQ(updated, 2u);

    auto rows = t.getRowsWhere([](const Row&){ return true; });
    ASSERT_EQ(rows.size(), 3u);
    EXPECT_EQ(asStr(rows.at(0),0), "new"); EXPECT_EQ(asInt(rows.at(0),1), 1);
    EXPECT_EQ(asStr(rows.at(1),0), "new"); EXPECT_EQ(asInt(rows.at(1),1), 2);
    EXPECT_EQ(asStr(rows.at(2),0), "y");   EXPECT_EQ(asInt(rows.at(2),1), 3);
}


TEST(Table, UpdateWhere_InvalidIndexThrows) {
    Table t{ schemaStrInt() };
    t.insertRow(rowSI("a", 1));

    std::vector<std::pair<size_t, RowValue>> assigns;
    assigns.emplace_back(2, RowValue{int64_t{7}}); // out of range

    try {
        (void)t.updateWhere([](const Row&){ return true; }, assigns);
        FAIL() << "Expected std::out_of_range";
    } catch (const std::out_of_range&) {
        SUCCEED();
    } catch (const std::exception& e) {
        FAIL() << "Expected std::out_of_range, got std::exception: " << e.what();
    } catch (...) {
        FAIL() << "Expected std::out_of_range, got non-std exception";
    }
}

TEST(Table, UpdateWhere_TypeMismatchThrows) {
    Table t{ schemaStrInt() };
    t.insertRow(rowSI("a", 1));

    // assign string to c2 (int)
    std::vector<std::pair<size_t, RowValue>> assigns;
    assigns.emplace_back(1, RowValue{std::string{"oops"}});

    try {
        (void)t.updateWhere([](const Row&){ return true; }, assigns);
        FAIL() << "Expected std::invalid_argument";
    } catch (const std::invalid_argument&) {
        SUCCEED();
    } catch (const std::exception& e) {
        FAIL() << "Expected std::invalid_argument, got std::exception: " << e.what();
    } catch (...) {
        FAIL() << "Expected std::invalid_argument, got non-std exception";
    }
}


TEST(Table, DeleteAllRows) {
    Table t{ schemaStrInt() };
    t.insertRow(rowSI("a", 1));
    t.insertRow(rowSI("b", 2));
    t.deleteAllRows();
    EXPECT_EQ(t.rowCount(), 0u);
}
