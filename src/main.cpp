#include "QApplication"
#include "XPlaneUDP.hpp"
#include "gui/main_widget.hpp"
#include "gui/themeColor.hpp"
#include "gui/main_window.hpp"
#include "utils/settingManage.hpp"

int main (int argc, char *argv[]) {
    QApplication app(argc, argv);
    // 主题
    setLightTheme(&app);
    setDarkTheme(&app);
    // 设置
    QApplication::setOrganizationName("Eyderoe");
    QApplication::setApplicationName("ChartNavigation");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    SettingsManager::instance();
    // 图标
    QIcon ico;
    ico.addFile(":/icon/resources/navi.png", QSize(256, 256));
    QApplication::setWindowIcon(ico);
    // 窗口
    main_window window;
    window.show();
    return QApplication::exec();
}
