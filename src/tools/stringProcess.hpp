#ifndef CHARTNAVIGATION_STRINGPROCESS_HPP
#define CHARTNAVIGATION_STRINGPROCESS_HPP

#include <string>

template <typename T>
concept StrT = std::convertible_to<T, std::string_view>;
template <typename R>
concept StrRangeT = std::ranges::forward_range<R> && StrT<std::ranges::range_reference_t<R>>;

template <StrRangeT R>
std::string join (R &&range, std::string_view sep);


/**
 * @brief 拼接字符串
 * @param range 待拼接的元素集合
 * @param sep 分隔符
 * @return 拼接后的字符串
 * @note C++23后应使用join_with()
 */
template <StrRangeT R>
std::string join (R &&range, const std::string_view sep) {
    auto begin = std::ranges::begin(range);
    auto end = std::ranges::end(range);
    if (begin == end)
        return {};
    size_t total_size = 0;
    size_t count = 0;
    for (const auto &s : range) {
        total_size += std::string_view(s).size();
        ++count;
    }
    total_size += sep.size() * (count - 1);
    std::string result;
    result.reserve(total_size);
    result.append(*begin);
    for (auto it = std::next(begin); it != end; ++it) {
        result.append(sep);
        result.append(*it);
    }
    return result;
}


#endif //CHARTNAVIGATION_STRINGPROCESS_HPP
