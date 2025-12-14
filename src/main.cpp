#include <QtWidgets>
#include "gui/main_widget.hpp"

int main (int argc, char *argv[]) {
    QApplication::setOrganizationName("Eyderoe");
    QApplication::setApplicationName("ChartNavigation");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QApplication a(argc, argv);
    main_widget widget;
    widget.show();
    return a.exec();
}
