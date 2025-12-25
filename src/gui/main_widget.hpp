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
        void setTheme (Qt::ColorScheme colorScheme) const;
    private:
        Ui::main_widget *ui;
        QPdfDocument *document;

        static std::vector<std::vector<double>> loadData (const QString &filePath);
        void readSettings ();
        void writeSettings () const;
    private Q_SLOTS:
        void on_chart_lineEdit_editingFinished () const; // 文件路径输入框 -> 加载PDF文档
        void on_dark_checkBox_clicked (bool checked) const; // 暗色主题选中框
        void on_follow_checkBox_clicked (bool checked); // 机模跟踪选中框
        void on_pin_checkBox_clicked (bool checked); // 程序窗口是否置顶
        void on_pageNum_spinBox_valueChanged (int pageNum); // PDF文档页数切换
};


#endif //CHARTNAVIGATION_MAIN_WIDGET_HPP
