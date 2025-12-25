#include "QApplication"
#include "gui/main_widget.hpp"
#include "gui/themeColor.hpp"

int main (int argc, char *argv[]) {
    QApplication app(argc, argv);
    // 主题
    setLightTheme(&app);
    setDarkTheme(&app);
    // 设置
    QApplication::setOrganizationName("Eyderoe");
    QApplication::setApplicationName("ChartNavigation");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    // 图标
    QIcon ico;
    ico.addFile(":/icon/resources/navi.png", QSize(256, 256));
    QApplication::setWindowIcon(ico);
    // 窗口
    main_widget widget;
    widget.setTheme(QApplication::styleHints()->colorScheme());
    widget.show();
    return QApplication::exec();
}
