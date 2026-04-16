#ifndef CHARTNAVIGATION_ENROUTE_WIDGET_HPP
#define CHARTNAVIGATION_ENROUTE_WIDGET_HPP

#include <QWidget>


QT_BEGIN_NAMESPACE
namespace Ui { class enroute_widget; }
QT_END_NAMESPACE

class enroute_widget : public QWidget {
Q_OBJECT

public:
    explicit enroute_widget(QWidget *parent = nullptr);
    ~enroute_widget() override;

private:
    Ui::enroute_widget *ui;
};


#endif //CHARTNAVIGATION_ENROUTE_WIDGET_HPP
