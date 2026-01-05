#include "QApplication"
#include "gui/main_widget.hpp"
#include "gui/themeColor.hpp"

// TODO 考虑在加载文件时直接读取一个文件的json数据(大改整个加载逻辑)
// TODO 根据文档类型设置阈值,程序5,机场10(或者在设置里面实现)
// TODO 多个实例时只有一个程序实例能更新数据,要么改XPlaneUDP要么改程序为单例或者再用websocket写一个试试
// TODO 更合理的暗色逻辑,且必须在QPdfView下实现
// TODO 切换时,页面相对位置锁定
// TODO 其他玩家/AI飞机
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
