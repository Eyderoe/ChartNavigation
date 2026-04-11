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
    private:
        Ui::options_widget *ui;

        void readSettings () const;
        void setFontSize () const;
        void closeEvent (QCloseEvent *event) override;
};


#endif //CHARTNAVIGATION_OPTIONS_WIDGET_HPP
