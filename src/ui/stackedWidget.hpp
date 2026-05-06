#ifndef CHARTNAVIGATION_STACKEDWIDGET_HPP
#define CHARTNAVIGATION_STACKEDWIDGET_HPP

#include <QStackedWidget>

class StackedWidget : public QStackedWidget {
        Q_OBJECT
    public:
        explicit StackedWidget (QWidget *parent = nullptr);
};

#endif //CHARTNAVIGATION_STACKEDWIDGET_HPP
