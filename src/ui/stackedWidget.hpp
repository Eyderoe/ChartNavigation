#ifndef CHARTNAVIGATION_STACKEDWIDGET_HPP
#define CHARTNAVIGATION_STACKEDWIDGET_HPP

#include <QStackedWidget>

class StackedWidget : public QStackedWidget {
        Q_OBJECT
    public:
        explicit StackedWidget (QWidget *parent = nullptr);
    private:
        std::map<std::string,char> turbuCate; // 尾流等级

        void readTurbuCate ();
};

#endif //CHARTNAVIGATION_STACKEDWIDGET_HPP
