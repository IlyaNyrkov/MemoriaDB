#include <gtest/gtest.h>
#include <memoria/Row.h>

#include <string>
#include <vector>
#include <variant>
#include <cstdint>
#include <stdexcept>

using namespace memoria;

static Row rowSI(std::string s, int64_t i) {
    std::vector<RowValue> v;
    v.emplace_back(std::move(s));
    v.emplace_back(std::in_place_type<int64_t>, i);
    return Row{ std::move(v) };
}

TEST(Row, AccessAndTypesConst) {
    const Row r = rowSI("foo", 42);
    const RowValue& v0 = r.at(0);
    const RowValue& v1 = r.at(1);

    EXPECT_TRUE(std::holds_alternative<std::string>(v0));
    EXPECT_TRUE(std::holds_alternative<int64_t>(v1));
    EXPECT_EQ(std::get<std::string>(v0), "foo");
    EXPECT_EQ(std::get<int64_t>(v1), 42);
}

TEST(Row, MutateThroughNonConstAt) {
    Row r = rowSI("x", 1);
    r.at(0).emplace<std::string>("bar");
    r.at(1).emplace<int64_t>(7);

    EXPECT_EQ(std::get<std::string>(r.at(0)), "bar");
    EXPECT_EQ(std::get<int64_t>(r.at(1)), 7);
}

TEST(Row, MutateInPlaceViaReference) {
    Row r = rowSI("hi", 5);
    std::get<std::string>(r.at(0)) += " there";
    EXPECT_EQ(std::get<std::string>(r.at(0)), "hi there");
}


TEST(Row, OutOfRangeThrows) {
    Row r = rowSI("a", 1);

    try {
        (void)r.at(2);   // out-of-bounds
        FAIL() << "Expected std::out_of_range";
    } catch (const std::out_of_range&) {
        SUCCEED();
    } catch (const std::exception& e) {
        FAIL() << "Expected std::out_of_range, got std::exception: " << e.what();
    } catch (...) {
        FAIL() << "Expected std::out_of_range, got non-std exception";
    }
}



TEST(Row, Size) {
    Row r = rowSI("a", 1);
    EXPECT_EQ(r.size(), 2u);
}
