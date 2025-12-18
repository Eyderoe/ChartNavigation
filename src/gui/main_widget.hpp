#ifndef CHARTNAVIGATION_MAIN_WIDGET_HPP
#define CHARTNAVIGATION_MAIN_WIDGET_HPP

#include <QWidget>
#include <QtPdf/QtPdf>
#include "gui/themeColor.hpp"

QT_BEGIN_NAMESPACE
namespace Ui
{
class main_widget;
}
QT_END_NAMESPACE


class main_widget final : public QWidget {
        Q_OBJECT
    public:
        explicit main_widget (QWidget *parent = nullptr);
        ~main_widget () override;
    public Q_SLOTS:
        void setTheme(Qt::ColorScheme colorScheme) const;
    private:
        Ui::main_widget *ui;
        QPdfDocument *document;
        static std::vector<std::vector<double>> loadData (const QString &filePath) ;
    private Q_SLOTS:
        void on_chart_lineEdit_editingFinished () const;
        void on_dark_checkBox_clicked(bool checked) const;
};


#endif //CHARTNAVIGATION_MAIN_WIDGET_HPP
