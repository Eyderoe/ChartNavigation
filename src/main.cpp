#include "gui/main_widget.hpp"
#include "gui/themeColor.hpp"

int main (int argc, char *argv[]) {
    QApplication app(argc, argv);
    // 主题：qt启动时会自己匹配主题 这里就设置静态属性
    setLightTheme(&app, true);
    setDarkTheme(&app, true);
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
    QObject::connect(app.styleHints(),&QStyleHints::colorSchemeChanged,&widget,&main_widget::setTheme);
    widget.show();
    return QApplication::exec();
}
