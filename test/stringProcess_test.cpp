#include <doctest.h>
#include "utils/stringProcess.hpp"

#include <vector>

TEST_CASE("join") {
    SUBCASE("vector") {
        std::vector<std::string_view> v1{"ab", "cd", "ef"};
        CHECK(join(v1,",") == "ab,cd,ef");
        CHECK(join(v1,"") == "abcdef");
        std::vector<std::string> v2{"ab", "cd", "ef"};
        v2.reserve(10);
        CHECK(join(v2,",") == "ab,cd,ef");
        CHECK(join(v2,"") == "abcdef");

        CHECK(join(std::vector<std::string_view>{}, ",").empty());
        CHECK(join(std::vector<std::string_view>{"only"}, ",") == "only");
        CHECK(join(std::vector<std::string_view>{"a", "", "c"}, ",") == "a,,c");
    }
}

TEST_CASE("split") {
    SUBCASE("std") {
        constexpr std::string_view input = ",a,,b,";
        CHECK((split(input, ",", false) == std::vector<std::string_view>{"", "a", "", "b", ""}));
        CHECK((split(input, ",") == std::vector<std::string_view>{"a", "b"}));
        CHECK((split("a  b\tc\n") == std::vector<std::string_view>{"a", "b", "c"}));
        CHECK((split("", ",").empty()));
        CHECK((split("one", ",") == std::vector<std::string_view>{"one"}));
        CHECK((split("a::b;c", ":;", true) == std::vector<std::string_view>{"a", "b", "c"}));
    }
    SUBCASE("qt") {
        const auto result = split(QStringView(u" a  b "));
        CHECK((result == QList<QStringView>{u"a", u"b"}));
        CHECK((split(QStringView(u"one\ttwo\nthree")) == QList<QStringView>{u"one", u"two", u"three"}));
        CHECK((split(QStringView(u"   ")).empty()));
        CHECK((split(QStringView(u"single")) == QList<QStringView>{u"single"}));
        CHECK((split(QStringView(u"a,b;c"), QStringView(u",;")) ==
               QList<QStringView>{u"a", u"b", u"c"}));
    }
}
