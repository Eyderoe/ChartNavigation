#include "main_widget.hpp"
#include "ui_main_widget.h"
#include "json.hpp"
#include "options_widget.hpp"
#include "ui/enhancedTree.hpp"
#include "ui/theme.hpp"
#include "services/settingManage.hpp"
#include "services/dataProvider.hpp"

#include <cmath>
#include <stdexcept>

namespace {
nlohmann::json readMappingFile (const QString &path) {
    QFile file(path);
    if (!file.exists())
        return {};
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << path << " 无法读取";
        return {};
    }
    try {
        return nlohmann::json::parse(file.readAll().toStdString());
    } catch (const nlohmann::json::exception &) {
        qWarning() << path << " 解析失败";
        return {};
    }
}

double mappingNumber (const nlohmann::json &object, const char *key) {
    const double value = object.at(key).get<double>();
    if (!std::isfinite(value))
        throw std::runtime_error("映射坐标非有限值");
    return value;
}
}


/**
 * @brief 程序启动时初始化文件树和文件夹选择框
 */
void main_widget::initFileTree () const {
    ui->treeWidget->clear();
    // 文件夹选择框
    const QString chartText = SettingsManager::instance().get(SettingsManager::chartFolder, "").toString();
    for (auto chartFolders = chartText.split('*'); const auto &folder : chartFolders) {
        QDir chartDir(folder);
        if (!chartDir.exists())
            continue;
        ui->folder_comboBox->addItem(chartDir.dirName(), chartDir.absolutePath());
    }
}

void main_widget::initConnect () {
    const auto &setting = SettingsManager::instance();
    // 缩放条联动
    ui->scale_verticalSlider->setRange(zoomMin * 100, zoomMax * 100);
    ui->scale_verticalSlider->setValue(100);
    connect(ui->scale_verticalSlider, &QSlider::valueChanged, this, [this](const int value) {
        const double factor = value / 100.0;
        ui->pdf_widget->zoomTo(factor);
    });
    connect(ui->pdf_widget, &PdfView::zoomFactor_changed, this, [this](double factor) {
        const int value = static_cast<int>(factor * 100);
        ui->scale_verticalSlider->blockSignals(true);
        ui->scale_verticalSlider->setValue(value);
        ui->scale_verticalSlider->blockSignals(false);
    });
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
    // 临时设置
    connect(&setting, qOverload<SettingsManager::TempKey, const QVariant&>(&SettingsManager::settingChanged), this,
            [this](const SettingsManager::TempKey key, const QVariant &val) {
                switch (key) {
                    case SettingsManager::suicide:
                        if (val.toBool())
                            saveSplitter();
                        break;
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
    // document 是 main_widget 的直接子对象，析构顺序晚于 ui 内的 PdfView。
    // 将连接上下文绑定到 PdfView，可保证 PdfView 一销毁连接就先断开；否则
    // QPdfDocument::~QPdfDocument() 发出的 statusChanged 会访问悬空的 ui 指针。
    documentStatusConnection = connect(document, &QPdfDocument::statusChanged, ui->pdf_widget, [this] {
        emit attachmentAvailabilityChanged(ui->pdf_widget->canAttachCurrentPage());
    });
    ui->pageNum_spinBox->setSpecialValueText("--");
    ui->pageNum_spinBox->setEnabled(false);
    // 设置
    initConnect();
    // 构建目录
    ui->treeWidget->setIconSize(QSize(48, 64));
    ui->treeWidget->setHeaderHidden(true);
    initFileTree();
}

main_widget::~main_widget () {
    // QWidget 会在派生类析构完成后才删除 Designer 创建的子控件，而 document
    // 是更早登记的直接子对象；QObject 默认析构顺序会因此先删 PdfView、后删
    // document。显式解除二者关系并先销毁 document，保证它在 close()/析构时
    // 发出的状态信号不会碰到已经析构的 QPdfView。
    if (document) {
        disconnect(documentStatusConnection);
        ui->pdf_widget->setDocument(nullptr);
        delete document;
        document = nullptr;
    }
    delete ui;
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
    emit attachmentAvailabilityChanged(false);
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
    ui->pdf_widget->fetchScale();
    on_pageNum_spinBox_valueChanged(0);
}

/**
 * @brief 加载文件夹
 * @param folder 文件夹
 */
void main_widget::loadFolder (const QString &folder) const {
    ui->treeWidget->loadFolder(folder);
}

/**
 * @brief 保存分割器位置
 */
void main_widget::saveSplitter () const {
    SettingsManager::instance().set(SettingsManager::spliterSta, ui->splitter->saveState(), true);
}

void main_widget::centerOwnAircraft () const {
    ui->pdf_widget->centerOwnAircraft();
}

void main_widget::setDataProvider (DataProvider *provider) const {
    ui->pdf_widget->setDataProvider(provider);
}

std::optional<AttachedChart> main_widget::currentPageAttachment () {
    return ui->pdf_widget->currentPageAttachment();
}

/**
 * @brief 从映射文件中加载仿射变换数据(一个机场文件的数据)
 */
void main_widget::loadPdfFileMapping () {
    fileData = {};
    tnaviData = {};
    // 文件夹可用性
    const QString mappingFolder = SettingsManager::instance().get(SettingsManager::dataFolder, "").toString();
    const QDir mappingDir(mappingFolder);
    if (!mappingDir.exists()) {
        return;
    }
    // Tmap 按基本名取页配置；Tnavi 按 ICAO-index_number 精确匹配。
    const QString baseName = QFileInfo(pdfFilePath).completeBaseName();
    const QString icao = baseName.left(4);
    const auto airportConfig = readMappingFile(mappingDir.filePath(icao + ".Tmap"));
    if (airportConfig.is_object()) {
        if (const auto it = airportConfig.find(baseName.toStdString()); it != airportConfig.end())
            fileData = it.value();
    }
    tnaviData = readMappingFile(mappingDir.filePath(icao + ".Tnavi"));
}

main_widget::MappingInfo main_widget::loadTnaviMapping () const {
    const auto *charts = &tnaviData;
    // 本地示例是数组；Navigraph 原型的顶层对象包含 charts 数组。
    if (tnaviData.is_object()) {
        const auto it = tnaviData.find("charts");
        if (it == tnaviData.end())
            return {{}, 0, 0, {}};
        charts = &it.value();
    }
    if (!charts->is_array())
        return {{}, 0, 0, {}};

    const QString baseName = QFileInfo(pdfFilePath).completeBaseName();
    for (const auto &chart : *charts) {
        if (!chart.is_object() || !chart.contains("index_number") || !chart["index_number"].is_string())
            continue;
        const QString chartName = baseName.left(4) + "-"
                                  + QString::fromStdString(chart["index_number"].get<std::string>());
        if (chartName != baseName)
            continue;
        try {
            if (!chart.value("is_georeferenced", false))
                return {{}, 0, 0, {}};
            const double width = mappingNumber(chart, "width");
            const double height = mappingNumber(chart, "height");
            if (width <= 0 || height <= 0)
                return {{}, 0, 0, {}};
            constexpr double pointsPerPixel = 72.0 / 360.0;
            const auto pixelRect = [&](const nlohmann::json &pixels) {
                const double x1 = mappingNumber(pixels, "x1"), x2 = mappingNumber(pixels, "x2");
                const double y1 = mappingNumber(pixels, "y1"), y2 = mappingNumber(pixels, "y2");
                if (x1 < 0 || x1 > width || x2 < 0 || x2 > width
                    || y1 < 0 || y1 > height || y2 < 0 || y2 > height || x1 == x2 || y1 == y2)
                    throw std::runtime_error("配准区域超出航图或退化");
                return QRectF(QPointF(x1 * pointsPerPixel, (height - y1) * pointsPerPixel),
                              QPointF(x2 * pointsPerPixel, (height - y2) * pointsPerPixel)).normalized();
            };
            const auto &boxes = chart.at("bounding_boxes");
            const auto &planview = boxes.at("planview");
            const auto &pixels = planview.at("pixels");
            const auto &latlng = planview.at("latlng");
            const QRectF bounds = pixelRect(pixels);
            const double lat1 = mappingNumber(latlng, "lat1"), lat2 = mappingNumber(latlng, "lat2");
            const double lon1 = mappingNumber(latlng, "lng1"), lon2 = mappingNumber(latlng, "lng2");
            if (std::abs(lat1) > 90 || std::abs(lat2) > 90 || std::abs(lon1) > 180
                || std::abs(lon2) > 180 || lat1 == lat2 || lon1 == lon2)
                return {{}, 0, 0, {}};
            // 经纬度与两个对角点一一对应，不能排序后重新配对。
            const double x1 = mappingNumber(pixels, "x1") * pointsPerPixel;
            const double x2 = mappingNumber(pixels, "x2") * pointsPerPixel;
            const double y1 = (height - mappingNumber(pixels, "y1")) * pointsPerPixel;
            const double y2 = (height - mappingNumber(pixels, "y2")) * pointsPerPixel;
            QPainterPath mappedArea;
            mappedArea.addRect(bounds);
            if (const auto it = boxes.find("insets"); it != boxes.end() && !it->is_null()) {
                if (!it->is_array())
                    return {{}, 0, 0, {}};
                for (const auto &inset : *it) {
                    QPainterPath excluded;
                    excluded.addRect(pixelRect(inset.at("pixels")));
                    mappedArea = mappedArea.subtracted(excluded);
                }
            }
            if (mappedArea.isEmpty())
                return {{}, 0, 0, {}};
            return {{{lat1, lon1, x1, y1}, {lat1, lon2, x2, y1},
                     {lat2, lon2, x2, y2}, {lat2, lon1, x1, y2}},
                    0, chart.value("category", "") == "APT" ? 10.0 : 5.0, mappedArea};
        } catch (const std::exception &ex) {
            qWarning() << chartName << " Tnavi 配准数据无效:" << ex.what();
            return {{}, 0, 0, {}};
        }
    }
    return {{}, 0, 0, {}};
}

/**
 * @brief 从缓存中加载仿射变换数据(一页的数据)
 * @param pageNum 页码
 * @brief {映射数据,旋转角度,阈值,配准区域}
 */
main_widget::MappingInfo main_widget::loadPdfPageMapping (const int pageNum) {
    // 页码可用性
    const nlohmann::basic_json<> *availableData{nullptr};
    for (const auto &pageConfig : fileData) {
        if (!pageConfig.is_array() || pageConfig.empty() || !pageConfig[0].is_object())
            continue;
        if (const auto &header = pageConfig[0]; header.contains("page") && header["page"] == pageNum - 1) {
            availableData = &pageConfig;
            break;
        }
    }
    if (availableData == nullptr)
        // Tnavi 每条记录是一张单页航图，不复用到 PDF 的后续页面。
        return pageNum == 1 ? loadTnaviMapping() : MappingInfo{{}, 0, 0, {}};
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
    return {data, (*availableData)[0]["rotate"], isAirport ? 10.0 : 5.0, {}};
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
    const auto [data, rotate, threshold, mappedArea] = loadPdfPageMapping(pageNumCorrect);
    ui->pdf_widget->loadMappingData(data, rotate, threshold, mappedArea);
    emit attachmentAvailabilityChanged(ui->pdf_widget->canAttachCurrentPage());
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
void main_widget::on_folder_comboBox_currentIndexChanged (const int index) const {
    loadFolder(ui->folder_comboBox->itemData(index).toString());
}
