#include "main_widget.hpp"
#include "ui_main_widget.h"
#include "json.hpp"
#include "options_widget.hpp"
#include "enhancedTree.hpp"
#include "gui/themeColor.hpp"

using namespace nlohmann;

/**
 * @brief 读取和配置设置
 */
void main_widget::readSettings () {
    const QSettings settings;
    // 窗口布局
    restoreGeometry(settings.value("main_widget_geometry").toByteArray());
    ui->splitter->restoreState(settings.value("main_splitter_state").toByteArray());
    // 居中
    const bool center = settings.value("center_on", false).toBool();
    ui->follow_checkBox->setCheckState(center ? Qt::Checked : Qt::Unchecked);
    ui->pdf_widget->setCenterOn(center);
    // 置顶
    const bool top = settings.value("pin_top", false).toBool();
    ui->pin_checkBox->setCheckState(top ? Qt::Checked : Qt::Unchecked);
    on_pin_checkBox_clicked(top);
    // 单文件输入框
    const bool singleFileDisable = settings.value("singleFileDisable", true).toBool();
    ui->chart_lineEdit->setHidden(singleFileDisable);
}

/**
 * @brief 析构时写入设置
 */
void main_widget::writeSettings () const {
    QSettings settings;
    // 窗口布局
    settings.setValue("main_widget_geometry", saveGeometry());
    settings.setValue("main_splitter_state", ui->splitter->saveState());
    // 居中
    settings.setValue("center_on", ui->follow_checkBox->isChecked());
    // 置顶
    settings.setValue("pin_top", ui->pin_checkBox->isChecked());
}

/**
 * @brief 程序启动时初始化文件树和文件夹选择框
 */
void main_widget::initFileTree () const {
    ui->treeWidget->clear();
    // 文件夹选择框
    const QSettings settings;
    const QString chartText = settings.value("chartFolder", "").toString();
    for (auto chartFolders = chartText.split('*'); const auto &folder : chartFolders) {
        QDir chartDir(folder);
        if (!chartDir.exists())
            continue;
        ui->folder_comboBox->addItem(chartDir.dirName(), chartDir.absolutePath());
    }
}

main_widget::main_widget (QWidget *parent) : QWidget(parent), ui(new Ui::main_widget) {
    // 构件初始化
    ui->setupUi(this);
    // PDF文档
    document = new QPdfDocument(this);
    ui->pdf_widget->setDocument(document);
    ui->pageNum_spinBox->setSpecialValueText("--");
    ui->pageNum_spinBox->setEnabled(false);
    // 设置
    readSettings();
    // 构建目录
    ui->treeWidget->setHeaderHidden(true);
    initFileTree();
}

main_widget::~main_widget () {
    writeSettings();
    delete ui;
}

/**
 * @brief 加载PDF文件
 * @param filePath 文件路径
 * @brief 两处调用(文本框编辑完/文件树结点点击)
 */
void main_widget::loadPdf (const QString &filePath) {
    // 先关闭文档
    document->close();
    pdfFilePath = "";
    ui->pdf_widget->loadMappingData({}, 0, 0);
    ui->pageNum_spinBox->setValue(0);
    ui->pageNum_spinBox->setEnabled(false);
    // 再尝试加载
    auto pdfPath = filePath;
    if (pdfPath.startsWith("\"") && pdfPath.endsWith("\"") && (pdfPath.size() >= 2))
        pdfPath = pdfPath.mid(1, pdfPath.length() - 2);
    if (!pdfPath.endsWith(".pdf", Qt::CaseInsensitive))
        return;
    if (const QFile file(pdfPath); !file.exists())
        return;
    pdfFilePath = pdfPath;
    ui->pageNum_spinBox->setEnabled(true);
    document->load(pdfPath);
    on_pageNum_spinBox_valueChanged(0);
}

/**
 * @brief 从映射文件中加载仿射变换数据
 * @param pageNum 页码(1起始)
 * @return 映射数据,旋转角度,阈值
 */
main_widget::MappingInfo main_widget::loadData (const int pageNum) {
    // 文件夹可用性
    const QSettings settings;
    const QString mappingFolder = settings.value("mappingFolder", "").toString();
    const QDir mappingDir(mappingFolder);
    if (!mappingDir.exists())
        return {};
    // 映射文件可用性 ZUCK.Tmap
    const QString baseName = QFileInfo(pdfFilePath).completeBaseName();
    const QString icao = baseName.left(4);
    const QString mappingFilePath = mappingDir.filePath(icao + ".Tmap");
    QFile mappingFile(mappingFilePath);
    if (!mappingFile.exists())
        return {};
    // 航图文件可用性 ZUCK-3P-01
    mappingFile.open(QIODevice::ReadOnly);
    QTextStream stream(&mappingFile);
    auto airportConfig = json::parse(stream.readAll().toUtf8().constData());
    if (const auto it = airportConfig.find(baseName.toStdString()); it == airportConfig.end())
        return {};
    // 页码可用性 1
    const auto &fileConfig = airportConfig[baseName.toStdString()];
    const basic_json<> *availableData{nullptr};
    for (const auto &pageConfig : fileConfig) {
        if (const auto &header = pageConfig[0]; header["page"] == pageNum - 1) {
            availableData = &pageConfig;
            break;
        }
    }
    if (availableData == nullptr)
        return {};
    // 装载数据
    std::vector<std::vector<double>> data;
    data.reserve(availableData->size() - 1);
    for (int i = 1; i < availableData->size(); ++i) {
        const auto &mapData = (*availableData)[i];
        double d1 = mapData[0];
        double d2 = mapData[1];
        double d3 = mapData[2];
        double d4 = mapData[3];
        data.push_back({d1, d2, d3, d4});
    }
    const bool isAirport = (*availableData)[0]["type"] == "parking"; // 机场图5 终端区10
    fileData = std::move(airportConfig[baseName.toStdString()]); // 加载数据至内存
    return {data, (*availableData)[0]["rotate"], isAirport ? 10.0 : 5.0};
}

/**
 * @brief 文件路径输入框 -> 加载PDF文档
 */
void main_widget::on_chart_lineEdit_editingFinished () {
    loadPdf(ui->chart_lineEdit->text());
}

/**
 * @brief 设置色彩主题
 * @param colorScheme 色彩主题
 */
void main_widget::setTheme (const Qt::ColorScheme colorScheme) const {
    if (colorScheme == Qt::ColorScheme::Dark) {
        setDarkTheme();
        ui->pdf_widget->setColorTheme(true);
        ui->dark_checkBox->setCheckState(Qt::Checked);
    } else {
        setLightTheme();
        ui->pdf_widget->setColorTheme(false);
        ui->dark_checkBox->setCheckState(Qt::Unchecked);
    }
}

/**
 * @brief 暗色主题选中框
 * @param checked 是否选中
 */
void main_widget::on_dark_checkBox_clicked (const bool checked) const {
    if (checked || (QApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark)) {
        setTheme(Qt::ColorScheme::Dark);
        ui->dark_checkBox->setCheckState(Qt::Checked);
    } else
        setTheme(Qt::ColorScheme::Light);
}

/**
 * @brief 机模跟踪选中框
 * @param checked 是否选中
 */
void main_widget::on_follow_checkBox_clicked (const bool checked) const {
    ui->pdf_widget->setCenterOn(checked);
}

/**
 * @brief 程序窗口是否置顶
 * @param checked 是否选中
 */
void main_widget::on_pin_checkBox_clicked (const bool checked) {
    if (checked)
        setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
    else
        setWindowFlags(windowFlags() & ~Qt::WindowStaysOnTopHint);
    show();
}

/**
 * @brief PDF文档页数切换
 * @param pageNum 页数(起始为1)
 */
void main_widget::on_pageNum_spinBox_valueChanged (const int pageNum) {
    // 数选框
    const int totalPages = ui->pdf_widget->document()->pageCount();
    if (totalPages == 0)
        return;
    const int pageNumCorrect = qBound(1, pageNum, totalPages);
    if (pageNumCorrect != pageNum) { // 说明页码经过修正
        ui->pageNum_spinBox->setValue(pageNumCorrect);
        return;
    }
    // 导航
    const auto pdf = ui->pdf_widget;
    pdf->pageNavigator()->jump(pageNumCorrect - 1, {0, 0}); // 不是很懂这个location
    // 映射数据加载
    const auto [data, rotate,threshold] = loadData(pageNumCorrect);
    ui->pdf_widget->loadMappingData(data, rotate, threshold);
}

/**
 * @brief 打开设置窗口
 */
void main_widget::on_license_radioButton_clicked () {
    const auto options = new options_widget(this);
    options->setWindowFlags(Qt::Window);
    options->show();
    options->setAttribute(Qt::WA_DeleteOnClose);
}

/**
 * @brief 双击文件树文件 -> 加载PDF文档
 * @param item 树节点
 * @param column 无用字段
 */
void main_widget::on_treeWidget_itemDoubleClicked (QTreeWidgetItem *item, int column) {
    const Node *node = dynamic_cast<Node*>(item);
    if (node->isFolder)
        return;
    loadPdf(node->baseDir);
}

/**
 * @brief 切换文件树文件夹
 * @param index 文件夹索引
 */
void main_widget::on_folder_comboBox_currentIndexChanged (const int index) const {
    const QSettings settings;
    ui->treeWidget->clear();
    traverseRead(ui->folder_comboBox->itemData(index).toString(), ui->treeWidget,
                 settings.value("onlyPdf", true).toBool());
}
