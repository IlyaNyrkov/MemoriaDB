//
// Created by Ilya Nyrkov on 22.08.25.
//

#include <gtest/gtest.h>
#include <memoria/Database.h>
#include <memoria/Schema.h>
#include <memoria/Row.h>

#include <string>
#include <stdexcept>

using namespace memoria;

static Schema schemaStrInt() {
    return Schema{{ {"c1", ColumnType::Str}, {"c2", ColumnType::Int} }};
}

TEST(Database, CreateAndGetTable) {
    Database db;
    db.createTable("t", schemaStrInt());

    ASSERT_TRUE(db.hasTable("t"));
    Table& t = db.getTable("t");
    EXPECT_EQ(t.getSchema().size(), 2u);

    // insert a row to prove it's the same table
    std::vector<RowValue> v;
    v.emplace_back(std::in_place_type<std::string>, "a");
    v.emplace_back(std::in_place_type<int64_t>, 1);
    t.insertRow(Row{v});
    EXPECT_EQ(t.rowCount(), 1u);
}


TEST(Database, GetTableMissingThrows) {
    Database db;

    try {
        (void)db.getTable("missing");
        FAIL() << "Expected std::out_of_range";
    } catch (const std::out_of_range&) {
        SUCCEED();
    } catch (const std::exception& e) {
        FAIL() << "Expected std::out_of_range, got std::exception: " << e.what();
    } catch (...) {
        FAIL() << "Expected std::out_of_range, got non-std exception";
    }
}

TEST(Database, CreateDuplicateTableRejected) {
    Database db;
    db.createTable("t", schemaStrInt());

    try {
        db.createTable("t", schemaStrInt());
        FAIL() << "Expected std::invalid_argument";
    } catch (const std::invalid_argument&) {
        SUCCEED();
    } catch (const std::exception& e) {
        FAIL() << "Expected std::invalid_argument, got std::exception: " << e.what();
    } catch (...) {
        FAIL() << "Expected std::invalid_argument, got non-std exception";
    }
}
