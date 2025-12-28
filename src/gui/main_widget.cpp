#include "main_widget.hpp"
#include "ui_main_widget.h"
#include "json.hpp"

using namespace nlohmann;

/**
 * @brief 读取和配置设置
 */
void main_widget::readSettings () {
    QSettings settings;
    // 窗口布局
    if (settings.contains("geometry"))
        restoreGeometry(settings.value("geometry").toByteArray());
    // 数据映射
    if (!settings.contains("mapping"))
        settings.setValue("mapping", "INOP");
    // 居中
    bool center{};
    if (settings.contains("center_on"))
        center = settings.value("center_on").toBool();
    if (center)
        ui->follow_checkBox->setCheckState(Qt::Checked);
    else
        ui->follow_checkBox->setCheckState(Qt::Unchecked);
    ui->pdf_widget->setCenterOn(center);
    // 置顶
    bool top{};
    if (settings.contains("pin_top"))
        top = settings.value("pin_top").toBool();
    if (top)
        ui->pin_checkBox->setCheckState(Qt::Checked);
    else
        ui->pin_checkBox->setCheckState(Qt::Unchecked);
    on_pin_checkBox_clicked(top);
}

/**
 * @brief 析构时写入设置
 */
void main_widget::writeSettings () const {
    QSettings settings;
    // 窗口布局
    settings.setValue("geometry", saveGeometry());
    // 居中
    settings.setValue("center_on", ui->follow_checkBox->isChecked());
    // 置顶
    settings.setValue("pin_top", ui->pin_checkBox->isChecked());
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
}

main_widget::~main_widget () {
    writeSettings();
    delete ui;
}

/**
 * @brief 从映射文件中加载仿射变换数据
 * @param pageNum 页码(1起始)
 * @return 存储的映射数据,旋转角度
 */
std::pair<std::vector<std::vector<double>>, double> main_widget::loadData (const int pageNum) const {
    // 文件夹可用性
    const QSettings settings;
    const QString mappingFolder = settings.value("mapping").toString();
    const QDir mappingDir(mappingFolder);
    if (!mappingDir.exists())
        return {};
    // 映射文件可用性 ZUCK.Tmap
    const QString baseName = QFileInfo(pdfFilePath).baseName();
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
        const auto &header = pageConfig[0];
        if (header["page"] == pageNum - 1) {
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
    return {data, (*availableData)[0]["rotate"]};
}

/**
 * @brief 文件路径输入框 -> 加载PDF文档
 */
void main_widget::on_chart_lineEdit_editingFinished () {
    // 先关闭文档
    document->close();
    pdfFilePath = "";
    ui->pdf_widget->loadMappingData({}, 0);
    ui->pageNum_spinBox->setValue(0);
    ui->pageNum_spinBox->setEnabled(false);
    // 再尝试加载
    auto pdfPath = ui->chart_lineEdit->text();
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
 * @brief 设置色彩主题
 * @param colorScheme
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
void main_widget::on_follow_checkBox_clicked (const bool checked) {
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
void main_widget::on_pageNum_spinBox_valueChanged (const int pageNum) const {
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
    const auto [data, rotate] = loadData(pageNumCorrect);
    ui->pdf_widget->loadMappingData(data, rotate);
}
