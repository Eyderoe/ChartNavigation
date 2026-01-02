#include "QApplication"
#include "gui/main_widget.hpp"
#include "gui/themeColor.hpp"

// TODO 支持多路径 形式待定
// TODO 完成设置 频率和过滤方法
// TODO 多个实例时只有一个实例能更新数据,要么改XPlaneUDP要么改程序为单例或者再用websocket写一个试试
// TODO 更合理的暗色逻辑,必须在QPdfView下实现
// TODO 页面相对位置锁定
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
