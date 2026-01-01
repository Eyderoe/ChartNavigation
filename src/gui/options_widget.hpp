#ifndef CHARTNAVIGATION_OPTIONS_WIDGET_HPP
#define CHARTNAVIGATION_OPTIONS_WIDGET_HPP

#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui
{
class options_widget;
}
QT_END_NAMESPACE

class options_widget final : public QWidget {
    Q_OBJECT
    public:
        explicit options_widget (QWidget *parent = nullptr);
        ~options_widget () override;
    private:
        Ui::options_widget *ui;

        void readSettings ();
        void writeSettings () const;
    private Q_SLOTS:
        void on_header_listWidget_currentRowChanged(int currentRow) const;
};


#endif //CHARTNAVIGATION_OPTIONS_WIDGET_HPP
