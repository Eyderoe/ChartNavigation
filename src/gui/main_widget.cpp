#include "main_widget.hpp"
#include "ui_main_widget.h"
#include "json.hpp"

using namespace nlohmann;

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
}

void main_widget::writeSettings () const {
    QSettings settings;
    // 窗口布局
    settings.setValue("geometry", saveGeometry());
    // 居中
    settings.setValue("center_on", ui->follow_checkBox->isChecked());
}

main_widget::main_widget (QWidget *parent) : QWidget(parent), ui(new Ui::main_widget) {
    // 构件初始化
    ui->setupUi(this);
    // PDF文档
    document = new QPdfDocument(this);
    ui->pdf_widget->setDocument(document);
    // 设置
    readSettings();
}

main_widget::~main_widget () {
    writeSettings();
    delete ui;
}

std::vector<std::vector<double>> main_widget::loadData (const QString &filePath) {
    // 检查文件夹可用性
    const QSettings settings;
    const QString mappingFolder = settings.value("mapping").toString();
    const QDir mappingDir(mappingFolder);
    if (!mappingDir.exists())
        return {};
    // 检查文件可用性
    const QString baseName = QFileInfo(filePath).baseName();
    const QString icao = baseName.left(4);
    const QString mappingFilePath = mappingDir.filePath(icao + ".Tmap");
    QFile mappingFile(mappingFilePath);
    if (!mappingFile.exists())
        return {};
    // 航图映射可用性
    mappingFile.open(QIODevice::ReadOnly);
    QTextStream stream(&mappingFile);
    json config = json::parse(stream.readAll().toUtf8().constData());
    if (const auto it = config.find(baseName.toStdString()); it == config.end())
        return {};
    // 装载数据
    std::vector<std::vector<double>> data;
    const auto &dataArray = config[baseName.toStdString()];
    data.reserve(dataArray.size());
    for (const auto &mapData : dataArray) {
        double d1 = mapData[0].get<double>();
        double d2 = mapData[1].get<double>();
        double d3 = mapData[2].get<double>();
        double d4 = mapData[3].get<double>();
        data.push_back({d1, d2, d3, d4});
    }
    return data;
}

void main_widget::on_chart_lineEdit_editingFinished () const {
    // 先关闭文档
    document->close();
    ui->pdf_widget->setDocSize(document->pagePointSize(0));
    ui->pdf_widget->loadMappingData({});
    // 再尝试加载
    auto pdfPath = ui->chart_lineEdit->text();
    if (pdfPath.startsWith("\"") && pdfPath.endsWith("\"") && (pdfPath.size() >= 2))
        pdfPath = pdfPath.mid(1, pdfPath.length() - 2);
    if (const QFile file(pdfPath); !file.exists())
        return;
    document->load(pdfPath);
    ui->pdf_widget->setDocSize(document->pagePointSize(0));
    ui->pdf_widget->loadMappingData(loadData(pdfPath));
}

void main_widget::setTheme (const Qt::ColorScheme colorScheme) const {
    if (colorScheme == Qt::ColorScheme::Dark) {
        setDarkTheme(nullptr);
        ui->pdf_widget->setColorTheme(true);
        ui->dark_checkBox->setCheckState(Qt::Checked);
    } else {
        setLightTheme(nullptr);
        ui->pdf_widget->setColorTheme(false);
        ui->dark_checkBox->setCheckState(Qt::Unchecked);
    }
}

void main_widget::on_dark_checkBox_clicked (const bool checked) const {
    if (checked || (QApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark)) {
        setTheme(Qt::ColorScheme::Dark);
        ui->dark_checkBox->setCheckState(Qt::Checked);
    } else
        setTheme(Qt::ColorScheme::Light);
}

void main_widget::on_follow_checkBox_clicked (const bool checked) {
    ui->pdf_widget->setCenterOn(checked);
}
