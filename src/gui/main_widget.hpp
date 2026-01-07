#ifndef CHARTNAVIGATION_MAIN_WIDGET_HPP
#define CHARTNAVIGATION_MAIN_WIDGET_HPP

#include <QWidget>
#include <QtPdf/QtPdf>

#include "json.hpp"

QT_BEGIN_NAMESPACE
namespace Ui
{
class main_widget;
}
QT_END_NAMESPACE


class main_widget final : public QWidget {
        Q_OBJECT
        struct MappingInfo {
            std::vector<std::vector<double>> mappingData;
            double rotateAngle;
            double threshold;
        };
    public:
        explicit main_widget (QWidget *parent = nullptr);
        ~main_widget () override;
    public Q_SLOTS:
        void setTheme (Qt::ColorScheme colorScheme) const;
    private:
        Ui::main_widget *ui;
        QPdfDocument *document;
        QString pdfFilePath{};
        nlohmann::json fileData{};

        void loadPdfFile (const QString &filePath);
        void loadPdfFileMapping();
        MappingInfo loadPdfPageMapping (int pageNum);
        void readSettings ();
        void writeSettings () const;
        void initFileTree () const;
    private Q_SLOTS:
        void on_chart_lineEdit_editingFinished (); // 文件路径输入框 -> 加载PDF文档
        void on_dark_checkBox_clicked (bool checked) const; // 暗色主题选中框
        void on_follow_checkBox_clicked (bool checked) const; // 机模跟踪选中框
        void on_pin_checkBox_clicked (bool checked); // 程序窗口是否置顶
        void on_pageNum_spinBox_valueChanged (int pageNum) ; // PDF文档页数切换
        void on_license_radioButton_clicked (); // 打开设置
        void on_treeWidget_itemDoubleClicked (QTreeWidgetItem *item, int column); // 文件树选择 -> 加载PDF文档
        void on_folder_comboBox_currentIndexChanged (int index) const; // 更换航图文件夹
};


#endif //CHARTNAVIGATION_MAIN_WIDGET_HPP
