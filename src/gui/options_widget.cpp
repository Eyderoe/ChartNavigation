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
    if (settings.contains("option_widget_geometry"))
        restoreGeometry(settings.value("option_widget_geometry").toByteArray());
    // 文件夹
    if (settings.contains("chartFolder"))
        ui->chartFolder_lineEdit->setText(settings.value("chartFolder").toString());
    if (settings.contains("mappingFolder"))
        ui->mappingFoler_lineEdit->setText(settings.value("mappingFolder").toString());
    // 单文件输入框
    ui->singleFileDisable_checkBox->setCheckState(Qt::Checked);
    if (settings.contains("singleFileDisable")) {
        if (const bool singleFileDisable = settings.value("singleFileDisable").toBool(); !singleFileDisable)
            ui->singleFileDisable_checkBox->setCheckState(Qt::Unchecked);
    }

}

void options_widget::writeSettings () const {
    QSettings settings;
    // 窗口布局
    settings.setValue("option_widget_geometry", saveGeometry());
    // 文件夹
    settings.setValue("chartFolder",ui->chartFolder_lineEdit->text());
    settings.setValue("mappingFolder",ui->mappingFoler_lineEdit->text());
    // 单文件输入框
    settings.setValue("singleFileDisable",ui->singleFileDisable_checkBox->isChecked());
}

void options_widget::on_header_listWidget_currentRowChanged (const int currentRow) const {
    ui->stackedWidget->setCurrentIndex(currentRow);
}
