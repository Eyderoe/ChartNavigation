#ifndef CHARTNAVIGATION_CONSTVALUE_HPP
#define CHARTNAVIGATION_CONSTVALUE_HPP

#include <QProcessEnvironment>

enum class MultiPlatform { winOS, linuxOS, macOS, androidOS };
#ifdef _WIN32
constexpr auto platform = MultiPlatform::winOS;
const bool inMacSandbox = false;
#elifdef __ANDROID__
constexpr auto platform = MultiPlatform::androidOS;
const bool inMacSandbox = false;
#elifdef __APPLE__
constexpr auto platform = MultiPlatform::macOS;
const bool inMacSandbox = QProcessEnvironment::systemEnvironment().contains("APP_SANDBOX_CONTAINER_ID");
#elifdef __linux__
constexpr auto platform = MultiPlatform::linuxOS;
const bool inMacSandbox = false;
#endif

constexpr double m2ft{3.28084};
constexpr double ft2m{1 / m2ft};
constexpr double nm2m{1852};
constexpr double m2nm{1 / nm2m};
constexpr double avgEarthRadius{6371008.8};

constexpr double NaN = std::numeric_limits<double>::quiet_NaN();

#endif //CHARTNAVIGATION_CONSTVALUE_HPP
