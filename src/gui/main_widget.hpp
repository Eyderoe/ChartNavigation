#ifndef CHARTNAVIGATION_MAIN_WIDGET_HPP
#define CHARTNAVIGATION_MAIN_WIDGET_HPP

#include <QWidget>
#include <QtPdf/QtPdf>
#include "ui/enhancedTree.hpp"

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

        void loadPdfFile (const QString &filePath);
        void loadFolder(const QString& folder);
    private:
        Ui::main_widget *ui;
        QPdfDocument *document;
        QString pdfFilePath{};
        nlohmann::json fileData{};

        void loadPdfFileMapping ();
        MappingInfo loadPdfPageMapping (int pageNum);
        void readSettings ();
        void writeSettings () const;
        void initFileTree () const;
        void initConnect();
    private Q_SLOTS:
        void on_pageNum_spinBox_valueChanged (int pageNum); // PDF文档页数切换
        void on_treeWidget_itemDoubleClicked (QTreeWidgetItem *item, int column); // 文件树选择 -> 加载PDF文档
        void on_folder_comboBox_currentIndexChanged (int index) const; // 更换航图文件夹
        void on_scale_verticalSlider_valueChanged (int value);
};


#endif //CHARTNAVIGATION_MAIN_WIDGET_HPP
