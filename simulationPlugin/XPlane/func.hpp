#ifndef X_PLUGIN_FUNC_HPP
#define X_PLUGIN_FUNC_HPP

#include <string>
#include <string_view>
#include <array>

inline std::string getString (const std::array<char, 512> &data, const int id) {
    std::string combine{};
    for (const char &chr : std::string_view(data.data() + id * 8, 8)) {
        if ((chr != 0) && (chr != 32))
            combine.push_back(chr);
    }
    return combine;
}

#endif //X_PLUGIN_FUNC_HPP
