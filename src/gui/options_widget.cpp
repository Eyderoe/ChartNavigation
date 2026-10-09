#include "options_widget.hpp"
#include "ui_options_widget.h"
#include "ui/pdfView.hpp"
#include "ui/enhancedTree.hpp"
#include "services/settingManage.hpp"

void options_widget::setFontSize () const {
    // 标准大小
    const int standardSize = ui->label_master_caution->font().pointSize();
    // 设置选项备注小一点 0.9
    for (QLabel *label : ui->scrollArea->findChildren<QLabel*>()) {
        if (label->objectName().contains("_remark")) {
            QFont font = label->font();
            font.setPointSize(static_cast<int>(standardSize * 0.9));
            label->setFont(font);
        }
    }
}

void options_widget::closeEvent (QCloseEvent *event) {
    SettingsManager &manager = SettingsManager::instance();
    manager.set(SettingsManager::OptionWidgetGeo, saveGeometry(), true);
    QWidget::closeEvent(event);
}

options_widget::options_widget (QWidget *parent) : QWidget(parent), ui(new Ui::options_widget) {
    ui->setupUi(this);
    readSettings();
    setFontSize();
    // 其他设置
    restoreGeometry(SettingsManager::instance().get(SettingsManager::OptionWidgetGeo, {}).toByteArray());
}

void options_widget::readSettings () const {
    SettingsManager &ins = SettingsManager::instance();
    // 样式
    ui->planeStyle_comboBox->setCurrentIndex(ins.getPending(SettingsManager::plane_style, 0).toInt() == 1 ? 1 : 0);
    ui->darkChartStyle_comboBox->setCurrentIndex(ins.getPending(SettingsManager::darkChartStyle, 0).toInt() == 1 ? 1 : 0);
    ui->projection_comboBox->setCurrentIndex(ins.getPending(SettingsManager::mapProjection, 0).toInt() == 1 ? 1 : 0);
    const int fileTreeStyle = ins.getPending(SettingsManager::fileTreeStyle, 0).toInt();
    ui->fileTreeStyle_comboBox->setCurrentIndex(fileTreeStyle >= 0 && fileTreeStyle <= 2 ? fileTreeStyle : 0);
    ui->onlyPdf_comboBox->setCurrentIndex(ins.getPending(SettingsManager::onlyDisplayPdf, true).toBool() ? 0 : 1);
    // 文件
    ui->chartFolder_lineEdit->setText(ins.getPending(SettingsManager::chartFolder, "").toString());
    ui->mappingFoler_lineEdit->setText(ins.getPending(SettingsManager::dataFolder, "").toString());
    ui->globeFoler_lineEdit->setText(ins.getPending(SettingsManager::globeFolder, "").toString());
    ui->airac_lineEdit->setText(ins.getPending(SettingsManager::airacPath, "").toString());
}

void options_widget::on_planeStyle_comboBox_currentIndexChanged (int index) {
    SettingsManager::instance().setPending(SettingsManager::plane_style, index);
}

void options_widget::on_fileTreeStyle_comboBox_currentIndexChanged (int index) {
    SettingsManager::instance().setPending(SettingsManager::fileTreeStyle, index);
}

void options_widget::on_darkChartStyle_comboBox_currentIndexChanged (int index) {
    SettingsManager::instance().setPending(SettingsManager::darkChartStyle, index);
}

void options_widget::on_projection_comboBox_currentIndexChanged (int index) {
    SettingsManager::instance().setPending(SettingsManager::mapProjection, index);
}

void options_widget::on_chartFolder_lineEdit_editingFinished () {
    SettingsManager::instance().setPending(SettingsManager::chartFolder, ui->chartFolder_lineEdit->text());
}

void options_widget::on_mappingFoler_lineEdit_editingFinished () {
    SettingsManager::instance().setPending(SettingsManager::dataFolder, ui->mappingFoler_lineEdit->text());
}

void options_widget::on_globeFoler_lineEdit_editingFinished () {
    SettingsManager::instance().setPending(SettingsManager::globeFolder, ui->globeFoler_lineEdit->text());
}

void options_widget::on_airac_lineEdit_editingFinished () {
    SettingsManager::instance().setPending(SettingsManager::airacPath, ui->airac_lineEdit->text());
}

void options_widget::on_onlyPdf_comboBox_currentIndexChanged (int index) {
    SettingsManager::instance().setPending(SettingsManager::onlyDisplayPdf, index == 0);
}
