#ifndef CHARTNAVIGATION_CONSTVALUE_HPP
#define CHARTNAVIGATION_CONSTVALUE_HPP


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

#endif //CHARTNAVIGATION_CONSTVALUE_HPP
