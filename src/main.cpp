#include <QApplication>
#include <QSharedMemory>

#include "XPlaneUDP.hpp"
#include "gui/main_widget.hpp"
#include "ui/themeColor.hpp"
#include "gui/main_window.hpp"
#include "utils/settingManage.hpp"

int main (int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName("Eyderoe");
    QApplication::setApplicationName("ChartNavigation");
    app.setAttribute(Qt::AA_DontShowIconsInMenus);
    // 单例程序
    static QSharedMemory sharedMemory("ChartNavigation_d7b233f1"); // ZUCK-1M-1
    if (!sharedMemory.create(1))
        return 99;
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [] () {
        if (sharedMemory.isAttached())
            sharedMemory.detach();
    });
    // 主题
    setLightTheme(&app);
    setDarkTheme(&app);
    // 设置
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
