#include "options_widget.hpp"
#include "ui_options_widget.h"


options_widget::options_widget (QWidget *parent) : QWidget(parent), ui(new Ui::options_widget) {
    ui->setupUi(this);
    readSettings();
}

options_widget::~options_widget () {
    writeSettings();
    delete ui;
}

void options_widget::readSettings () {
    const QSettings settings;
    // 窗口布局
    restoreGeometry(settings.value("option_widget_geometry").toByteArray());
    // 文件夹
    ui->chartFolder_lineEdit->setText(settings.value("chartFolder", "").toString());
    ui->mappingFoler_lineEdit->setText(settings.value("mappingFolder", "").toString());
    ui->onlyPdf_checkBox->setCheckState(settings.value("onlyPdf", true).toBool() ? Qt::Checked : Qt::Unchecked);
    // 单文件输入框
    const bool singleFileDisable = settings.value("singleFileDisable", true).toBool();
    ui->singleFileDisable_checkBox->setCheckState(singleFileDisable ? Qt::Checked : Qt::Unchecked);
    // 映射
    const int xpFreq = settings.value("xp_freq", 1).toInt();
    ui->xpFreq_spinBox->setValue(xpFreq);
    const int centerFreq = settings.value("center_freq", 1).toInt();
    ui->centerFreq_spinBox->setValue(centerFreq);
}

void options_widget::writeSettings () const {
    QSettings settings;
    // 窗口布局
    settings.setValue("option_widget_geometry", saveGeometry());
    // 文件夹
    settings.setValue("chartFolder", ui->chartFolder_lineEdit->text());
    settings.setValue("mappingFolder", ui->mappingFoler_lineEdit->text());
    settings.setValue("onlyPdf", ui->onlyPdf_checkBox->isChecked());
    // 单文件输入框
    settings.setValue("singleFileDisable", ui->singleFileDisable_checkBox->isChecked());
    // 映射
    settings.setValue("xp_freq", ui->xpFreq_spinBox->value());
    settings.setValue("center_freq", ui->centerFreq_spinBox->value());
}

void options_widget::on_header_listWidget_currentRowChanged (const int currentRow) const {
    ui->stackedWidget->setCurrentIndex(currentRow);
}
