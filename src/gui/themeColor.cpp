#include "themeColor.hpp"

void setDarkTheme (QApplication *a, const bool skip) {
    static QApplication *app{a};
    if (skip)
        return;
    QPalette p;
    p.setColor(QPalette::Window, QColor(30, 30, 30));
    p.setColor(QPalette::WindowText, Qt::white);
    p.setColor(QPalette::Base, QColor(25, 25, 25));
    p.setColor(QPalette::AlternateBase, QColor(40, 40, 40));
    p.setColor(QPalette::ToolTipBase, Qt::white);
    p.setColor(QPalette::ToolTipText, Qt::white);
    p.setColor(QPalette::Text, Qt::white);
    p.setColor(QPalette::Button, QColor(45, 45, 45));
    p.setColor(QPalette::ButtonText, Qt::white);
    p.setColor(QPalette::BrightText, Qt::red);
    p.setColor(QPalette::Highlight, QColor(90, 90, 200));
    p.setColor(QPalette::HighlightedText, Qt::black);
    app->setPalette(p);
}

void setLightTheme (QApplication *a, const bool skip) {
    static QApplication *app{a};
    static QPalette p = app->palette();
    if (skip)
        return;
    app->setPalette(p);
}
