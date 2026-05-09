#include <QApplication>

#include "XPlaneUDP.hpp"
#include "gui/main_widget.hpp"
#include "ui/themeColor.hpp"
#include "gui/main_window.hpp"
#include "utils/constValue.hpp"
#include "utils/settingManage.hpp"

int main (int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName("Eyderoe");
    QApplication::setApplicationName("ChartNavigation");
    QApplication::setAttribute(Qt::AA_DontShowIconsInMenus);
    // 翻译
    auto *qtTranslator = new QTranslator(&app);
    if (qtTranslator->load(":/trans/translation/qtbase_zh_CN.qm"))
        QApplication::installTranslator(qtTranslator);
    // 主题
    setLightTheme(&app);
    setDarkTheme(&app);
    // 设置
    QSettings::setDefaultFormat(QSettings::IniFormat);
    SettingsManager::instance();
    qDebug() << "QSettings path: " << QSettings().fileName();
    // 图标
    QIcon ico;
    ico.addFile(":/icon/resources/navi.png", QSize(256, 256));
    QApplication::setWindowIcon(ico);
    // 窗口
    main_window window;
    if constexpr (platform == MultiPlatform::androidOS) {
        window.setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
        window.showFullScreen();
    } else {
        window.show();
    }
    return QApplication::exec();
}
