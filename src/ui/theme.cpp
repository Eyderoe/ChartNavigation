#include "theme.hpp"

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

/**
 * @brief 去掉在qdarkstyle下comboBox选项前面那一个框并匹配长度以容纳所有字符
 * @param comboBox 选项框
 * @param extra 额外空间
 * @note 发病机制不明
 */
void expandComboBox (const QComboBox *comboBox, const int extra) {
    auto *view = comboBox->view();
    view->setItemDelegate(new QStyledItemDelegate(view));
    view->setTextElideMode(Qt::ElideNone);
    const int width = view->sizeHintForColumn(0);
    view->setMinimumWidth(width + extra);
}
