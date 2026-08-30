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
    // 文件
    ui->chartFolder_lineEdit->setText(ins.get(SettingsManager::chartFolder, "").toString());
    ui->mappingFoler_lineEdit->setText(ins.get(SettingsManager::dataFolder, "").toString());
    ui->globeFoler_lineEdit->setText(ins.get(SettingsManager::globeFolder, "").toString());
    ui->onlyPdf_comboBox->setCurrentIndex(ins.get(SettingsManager::onlyDisplayPdf, true).toBool() ? 0 : 1);
}

void options_widget::on_chartFolder_lineEdit_editingFinished () {
    SettingsManager::instance().set(SettingsManager::chartFolder, ui->chartFolder_lineEdit->text(), true);
}

void options_widget::on_mappingFoler_lineEdit_editingFinished () {
    SettingsManager::instance().set(SettingsManager::dataFolder, ui->mappingFoler_lineEdit->text(), true);
}

void options_widget::on_globeFoler_lineEdit_editingFinished () {
    SettingsManager::instance().set(SettingsManager::globeFolder, ui->globeFoler_lineEdit->text(), true);
}

void options_widget::on_airac_lineEdit_editingFinished () {
    SettingsManager::instance().set(SettingsManager::airacPath, ui->airac_lineEdit->text(), true);
}

void options_widget::on_onlyPdf_comboBox_currentIndexChanged (int index) {
    SettingsManager::instance().set(SettingsManager::onlyDisplayPdf, index == 0, true);
}
