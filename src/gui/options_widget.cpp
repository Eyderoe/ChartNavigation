#include "options_widget.hpp"
#include "ui_options_widget.h"
#include "pdfView.hpp"
#include "enhancedTree.hpp"

void options_widget::setFontSize () const {
    // 标准大小
    const int standardSize = ui->label_master_caution->font().pointSize();
    // 设置选项备注小一点 0.9
    QList<QLabel*> labels = ui->common_page->findChildren<QLabel*>();
    for (QLabel *label : labels) {
        if (label->objectName().contains("_remark")) {
            QFont font = label->font();
            font.setPointSize(static_cast<int>(standardSize * 0.9));
            label->setFont(font);
        }
    }
}

options_widget::options_widget (QWidget *parent) : QWidget(parent), ui(new Ui::options_widget) {
    ui->setupUi(this);
    readSettings();
    setFontSize();
}

options_widget::~options_widget () {
    writeSettings();
    delete ui;
}

void options_widget::readSettings () {
    const QSettings settings;
    // 窗口布局
    restoreGeometry(settings.value("option_widget_geometry").toByteArray());
    // 数据源
    ui->dataSource_comboBox->setCurrentIndex(settings.value("data_source", 0).toInt());
    // 文件夹
    ui->chartFolder_lineEdit->setText(settings.value("chartFolder", "").toString());
    ui->mappingFoler_lineEdit->setText(settings.value("mappingFolder", "").toString());
    const int displayFile = settings.value("displayFile", 0).toInt();
    ui->onlyPdf_comboBox->setCurrentIndex(displayFile);
    // 单文件输入框
    const bool singleFileDisable = settings.value("singleFileDisable", true).toBool();
    ui->singleFileDisable_checkBox->setCheckState(singleFileDisable ? Qt::Checked : Qt::Unchecked);
    // 缩放比条
    const bool scaleBarEnable = settings.value("scaleBarEnable", false).toBool();
    ui->pdfScaleBar_checkBox->setCheckState(scaleBarEnable ? Qt::Checked : Qt::Unchecked);
    // 映射
    const int xpFreq = settings.value("xp_freq", 1).toInt();
    ui->xpFreq_spinBox->setValue(xpFreq);
    const int centerFreq = settings.value("center_freq", 1).toInt();
    ui->centerFreq_spinBox->setValue(centerFreq);
    // TCAS 范围、高度显示
    const int tacsMode = settings.value("tcasMode", 0).toInt();
    const int altMode = settings.value("altMode", 0).toInt();
    ui->tcas_comboBox->setCurrentIndex(tacsMode);
    ui->alt_comboBox->setCurrentIndex(altMode);
}

void options_widget::writeSettings () const {
    QSettings settings;
    // 窗口布局
    settings.setValue("option_widget_geometry", saveGeometry());
    // 数据源
    settings.setValue("data_source", ui->dataSource_comboBox->currentIndex());
    // 文件夹
    settings.setValue("chartFolder", ui->chartFolder_lineEdit->text());
    settings.setValue("mappingFolder", ui->mappingFoler_lineEdit->text());
    settings.setValue("displayFile", ui->onlyPdf_comboBox->currentIndex());
    // 单文件输入框
    settings.setValue("singleFileDisable", ui->singleFileDisable_checkBox->isChecked());
    // 缩放比条
    settings.setValue("scaleBarEnable", ui->pdfScaleBar_checkBox->isChecked());
    // 映射
    settings.setValue("xp_freq", ui->xpFreq_spinBox->value());
    settings.setValue("center_freq", ui->centerFreq_spinBox->value());
    // TCAS 范围
    settings.setValue("tcasMode", ui->tcas_comboBox->currentIndex());
    settings.setValue("altMode", ui->alt_comboBox->currentIndex());
}

void options_widget::on_header_listWidget_currentRowChanged (const int currentRow) const {
    ui->stackedWidget->setCurrentIndex(currentRow);
}
