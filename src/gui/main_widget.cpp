#include "main_widget.hpp"
#include "ui_main_widget.h"
#include "json.hpp"
#include "options_widget.hpp"
#include "ui/enhancedTree.hpp"
#include "ui/themeColor.hpp"
#include "utils/settingManage.hpp"

using namespace nlohmann;

/**
 * @brief 读取和配置设置
 */
void main_widget::readSettings () {
    const QSettings settings;
    // 窗口布局
    restoreGeometry(settings.value("main_widget_geometry").toByteArray());
    ui->splitter->restoreState(settings.value("main_splitter_state").toByteArray());
    // 缩放比条
    const bool scaleBarEnable = settings.value("scaleBarEnable", false).toBool();
    ui->scale_verticalSlider->setHidden(!scaleBarEnable);
    // TCAS 范围、高度显示
    const int tacsMode = settings.value("tcasMode", 0).toInt();
    const int altMode = settings.value("altMode", 0).toInt();
    ui->pdf_widget->setTcasInfo(static_cast<TcasMode>(tacsMode), static_cast<AltMode>(altMode));
}

/**
 * @brief 析构时写入设置
 */
void main_widget::writeSettings () const {
    QSettings settings;
    // 窗口布局
    settings.setValue("main_widget_geometry", saveGeometry());
    settings.setValue("main_splitter_state", ui->splitter->saveState());
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

void main_widget::initConnect () {
    const auto &setting = SettingsManager::instance();
    // 存储设置
    connect(&setting, qOverload<SettingsManager::ConstKey, const QVariant&>(&SettingsManager::settingChanged), this,
            [this](const SettingsManager::ConstKey key, const QVariant &val) {
                switch (key) {
                    case SettingsManager::scaleBarEnable: {
                        ui->scale_verticalSlider->setHidden(!val.toBool());
                        break;
                    }
                    case SettingsManager::spliterSta: {
                        ui->splitter->restoreState(val.toByteArray());
                        break;
                    }
                    default:
                        break;
                }
            });
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
    initConnect();
    // 构建目录
    ui->treeWidget->setIconSize(QSize(48, 64));
    ui->treeWidget->setHeaderHidden(true);
    initFileTree();
}

/**
 * @brief 加载PDF文件
 * @param filePath 文件路径
 * @brief 两处调用(文本框编辑完/文件树结点点击)
 */
void main_widget::loadPdfFile (const QString &filePath) {
    // 先关闭文档
    ui->pdf_widget->pageNavigator()->jump(0, {0, 0});
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
    loadPdfFileMapping();
    on_pageNum_spinBox_valueChanged(0);
}

/**
 * @brief 加载文件夹
 * @param folder 文件夹
 */
void main_widget::loadFolder (const QString &folder) {
    const auto tree = static_cast<Tree*>(ui->treeWidget);
    tree->loadFolder(folder);
}

/**
 * @brief 保存分割buju
 * @note 改为 QMainWindows 后, closeEvent 无效
 */
void main_widget::saveSplitter () const {
    SettingsManager::instance().set(SettingsManager::spliterSta, ui->splitter->saveState(), true);
}

/**
 * @brief 从映射文件中加载仿射变换数据(一个机场文件的数据)
 */
void main_widget::loadPdfFileMapping () {
    // 文件夹可用性
    const QSettings settings;
    const QString mappingFolder = settings.value("mappingFolder", "").toString();
    const QDir mappingDir(mappingFolder);
    if (!mappingDir.exists()) {
        fileData = {};
        return;
    }
    // 映射文件可用性 ZUCK.Tmap
    const QString baseName = QFileInfo(pdfFilePath).completeBaseName();
    const QString icao = baseName.left(4);
    const QString mappingFilePath = mappingDir.filePath(icao + ".Tmap");
    QFile mappingFile(mappingFilePath);
    if (!mappingFile.exists()) {
        fileData = {};
        return;
    }
    // 航图文件可用性 ZUCK-3P-01
    mappingFile.open(QIODevice::ReadOnly);
    QTextStream stream(&mappingFile);
    auto airportConfig = json::parse(stream.readAll().toUtf8().constData());
    const auto it = airportConfig.find(baseName.toStdString());
    if (it == airportConfig.end()) {
        fileData = {};
        return;
    }
    fileData = std::move(it.value());
}

/**
 * @brief 从缓存中加载仿射变换数据(一页的数据)
 * @param pageNum 页码
 * @brief {映射数据,旋转角度,阈值}
 */
main_widget::MappingInfo main_widget::loadPdfPageMapping (const int pageNum) {
    // 页码可用性
    const basic_json<> *availableData{nullptr};
    for (const auto &pageConfig : fileData) {
        if (const auto &header = pageConfig[0]; header["page"] == pageNum - 1) {
            availableData = &pageConfig;
            break;
        }
    }
    if (availableData == nullptr)
        return {{}, 0, 0};
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
    const bool isAirport = (*availableData)[0]["type"] == "parking"; // 机场图10 终端区5
    return {data, (*availableData)[0]["rotate"], isAirport ? 10.0 : 5.0};
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
    if (pageNumCorrect != pageNum) {
        ui->pageNum_spinBox->setValue(pageNumCorrect);
        return;
    }
    // 导航
    const auto pdf = ui->pdf_widget;
    pdf->pageNavigator()->jump(pageNumCorrect - 1, {0, 0}); // 不是很懂这个location
    // 映射数据加载
    const auto [data, rotate,threshold] = loadPdfPageMapping(pageNumCorrect);
    ui->pdf_widget->loadMappingData(data, rotate, threshold);
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
    loadPdfFile(node->baseDir);
}

/**
 * @brief 切换文件树文件夹
 * @param index 文件夹索引
 */
void main_widget::on_folder_comboBox_currentIndexChanged (const int index) {
    loadFolder(ui->folder_comboBox->itemData(index).toString());
}

void main_widget::on_scale_verticalSlider_valueChanged (int value) {}
