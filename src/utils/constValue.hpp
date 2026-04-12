#ifndef CHARTNAVIGATION_CONSTVALUE_HPP
#define CHARTNAVIGATION_CONSTVALUE_HPP

enum class MultiPlatform { win, linux, mac };
#ifdef _WIN32
constexpr auto platform = MultiPlatform::win;
#elifdef __linux__
constexpr auto platform = MultiPlatform::linux;
#elifdef __APPLE__
constexpr auto platform = MultiPlatform::mac;
#endif

#ifdef Q_OS_MAC
const bool inMacSandbox = QProcessEnvironment::systemEnvironment().contains("APP_SANDBOX_CONTAINER_ID");
#endif

constexpr double m2ft{3.28084};
constexpr double nm2m{1852};
constexpr double avgEarthRadius{6371008.8};

constexpr double zoomMin{0.2};
constexpr double zoomMax{4};

constexpr int hashHeaderLength{1024 * 12};

#endif //CHARTNAVIGATION_CONSTVALUE_HPP
