#ifndef CHARTNAVIGATION_CONSTVALUE_HPP
#define CHARTNAVIGATION_CONSTVALUE_HPP


#include <cmath>
#include <limits>
#include <utility>


enum class MultiPlatform { winOS, linuxOS, macOS, androidOS };
#ifdef _WIN32
constexpr auto platform = MultiPlatform::winOS;
#elifdef __ANDROID__
constexpr auto platform = MultiPlatform::androidOS;
#elifdef __APPLE__
constexpr auto platform = MultiPlatform::macOS;
#elifdef __linux__
constexpr auto platform = MultiPlatform::linuxOS;
#endif

constexpr double m2ft{3.28084};
constexpr double nm2m{1852};
constexpr double avgEarthRadius{6371008.8};

constexpr double NaN = std::numeric_limits<double>::quiet_NaN();
constexpr double Inf = std::numeric_limits<double>::infinity();


template <typename T>
constexpr bool finite (const T &value) {
    return std::isfinite(value);
}
template <typename T, typename U>
constexpr bool finite (const std::pair<T, U> &value) {
    return finite(value.first) && finite(value.second);
}
/**
 * @brief 判断所有形参 均有限 吗
 */
template <typename... Args>
constexpr bool allFinite (const Args &... args) {
    return (... && finite(args));
}

#endif //CHARTNAVIGATION_CONSTVALUE_HPP
