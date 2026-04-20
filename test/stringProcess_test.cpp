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
    }
}
