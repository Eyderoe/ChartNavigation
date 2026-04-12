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
    protected:
        void closeEvent (QCloseEvent *event) override;
    private:
        Ui::options_widget *ui;

        void readSettings () const;
        void setFontSize () const;
    private slots:
        static void on_chartFolder_lineEdit_textEdited(const QString &arg1);
        static void on_mappingFoler_lineEdit_textEdited(const QString &arg1);
        static void on_onlyPdf_comboBox_currentIndexChanged(int index);
};


#endif //CHARTNAVIGATION_OPTIONS_WIDGET_HPP
