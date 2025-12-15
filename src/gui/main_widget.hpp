#ifndef CHARTNAVIGATION_MAIN_WIDGET_HPP
#define CHARTNAVIGATION_MAIN_WIDGET_HPP

#include <QWidget>
#include <QtPdf/QtPdf>
#include <QtPdfWidgets/QPdfView>

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
    private:
        Ui::main_widget *ui;
        QPdfDocument *document;
        static std::vector<std::vector<double>> loadData (const QString &filePath) ;
    private slots:
        void on_chart_lineEdit_editingFinished () const;
};


#endif //CHARTNAVIGATION_MAIN_WIDGET_HPP
