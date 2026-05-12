#include "stringProcess.hpp"
#include <ranges>


/**
 * @brief 将字符串转换为十六进制
 * @param data 数据头
 * @param length 长度
 * @return 转换后的字符串
 */
std::string toHex (const char *data, const int length) {
    std::string base;
    base.reserve(length * 3);
    for (int i = 0; i < length; i++)
        base += std::format("{:02X} ", data[i]);
    return base;
}

/**
 * @brief 分割字符串，类似 Python
 * @param str 原始字符串
 * @param delimiters 分隔符集合，默认为空白字符
 * @param skipEmpty 是否跳过空字符串
 * @return 包含分割后子字符串的 vector<string_view>
 */
std::vector<std::string_view> split (const std::string_view str, const std::string_view delimiters,
                                     const bool skipEmpty) {
    std::vector<std::string_view> result;
    auto first = str.find_first_not_of(delimiters);
    while (first != std::string_view::npos) {
        auto last = str.find_first_of(delimiters, first);
        if (last == std::string_view::npos) {
            last = str.size();
        }
        if (!skipEmpty || last > first) {
            result.emplace_back(str.substr(first, last - first));
        }
        first = str.find_first_not_of(delimiters, last);
    }
    return result;
}
