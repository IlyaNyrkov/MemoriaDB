//
// Created by Ilya Nyrkov on 22.08.25.
//

#include <gtest/gtest.h>
#include <memoria/Schema.h>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

using namespace memoria;

static Schema makeSchema() {
    std::vector<Column> cols = {{"c1", ColumnType::Str}, {"c2", ColumnType::Int}};
    return Schema{std::move(cols)};
}

TEST(Schema, SizeAndColumns) {
    Schema s = makeSchema();
    EXPECT_EQ(s.size(), 2u);
    ASSERT_EQ(s.columns().size(), 2u);
    EXPECT_EQ(s.columns().at(0).name, "c1");
    EXPECT_EQ(s.columns().at(1).name, "c2");
}

TEST(Schema, IndexLookup) {
    Schema s = makeSchema();
    const auto i1 = s.index_of("c1");
    const auto i2 = s.index_of("c2");
    const auto ix = s.index_of("nope");

    ASSERT_TRUE(i1.has_value());
    ASSERT_TRUE(i2.has_value());
    EXPECT_EQ(*i1, 0u);
    EXPECT_EQ(*i2, 1u);
    EXPECT_FALSE(ix.has_value());

    EXPECT_NO_THROW((void)s.require_index("c1"));
}

TEST(Schema, DefaultValues) {
    Schema s = makeSchema();
    auto v0 = s.default_value(0);
    auto v1 = s.default_value(1);

    EXPECT_TRUE(std::holds_alternative<std::string>(v0));
    EXPECT_EQ(std::get<std::string>(v0), "");
    EXPECT_TRUE(std::holds_alternative<int64_t>(v1));
    EXPECT_EQ(std::get<int64_t>(v1), 0);
}

TEST(Schema, DuplicateColumnsRejected) {
    std::vector<Column> cols = {{"c", ColumnType::Int}, {"c", ColumnType::Str}};

    try {
        (void)Schema{cols};
        FAIL() << "Expected std::invalid_argument";
    } catch (const std::invalid_argument&) {
        SUCCEED();
    } catch (const std::exception& e) {
        FAIL() << "Expected std::invalid_argument, got std::exception: " << e.what();
    } catch (...) {
        FAIL() << "Expected std::invalid_argument, got non-std exception";
    }
}
