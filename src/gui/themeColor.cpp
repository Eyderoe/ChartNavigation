#include "themeColor.hpp"
#include "QStyle"

// TODO 未完全解决Windows暗色主题 只能说很奇怪的跑起来了
void setDarkTheme (QApplication *a) {
    static QApplication *app{a};
    if (QApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark)
        QApplication::setPalette(QApplication::style()->standardPalette());
    app->setStyleSheet("");
    static QString sheet{};
    if (a != nullptr) {
        QFile qss(":/css/resources/qdarkstyle/dark/darkstyle.qss");
        qss.open(QFile::ReadOnly | QFile::Text);
        QTextStream ts(&qss);
        sheet = ts.readAll();
    }
    app->setStyleSheet(sheet);
}

void setLightTheme (QApplication *a) {
    static QApplication *app{a};
    app->setStyleSheet("");
    static QString sheet{};
    if (a != nullptr) {
        QFile qss(":/css/resources/qdarkstyle/light/lightstyle.qss");
        qss.open(QFile::ReadOnly | QFile::Text);
        QTextStream ts(&qss);
        sheet = ts.readAll();
    }
    app->setStyleSheet(sheet);
}
