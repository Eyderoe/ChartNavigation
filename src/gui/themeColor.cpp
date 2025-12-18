#include "themeColor.hpp"
#include "QStyle"


void setDarkTheme (QApplication *a) {
    static QApplication *app{a};
    app->setStyleSheet("");
    QApplication::setPalette(QApplication::style()->standardPalette());
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
    QApplication::setPalette(QApplication::style()->standardPalette());
    static QString sheet{};
    if (a != nullptr) {
        QFile qss(":/css/resources/qdarkstyle/light/lightstyle.qss");
        qss.open(QFile::ReadOnly | QFile::Text);
        QTextStream ts(&qss);
        sheet = ts.readAll();
    }
    app->setStyleSheet(sheet);
}
