#include "options_widget.hpp"
#include "ui_options_widget.h"
#include "ui/pdfView.hpp"
#include "ui/enhancedTree.hpp"
#include "utils/settingManage.hpp"

void options_widget::setFontSize () const {
    // 标准大小
    const int standardSize = ui->label_master_caution->font().pointSize();
    // 设置选项备注小一点 0.9
    QList<QLabel*> labels = ui->scrollArea->findChildren<QLabel*>();
    for (QLabel *label : labels) {
        if (label->objectName().contains("_remark")) {
            QFont font = label->font();
            font.setPointSize(static_cast<int>(standardSize * 0.9));
            label->setFont(font);
        }
    }
}

void options_widget::initConnect () {
    const auto &setting = SettingsManager::instance();
    // 存储设置
    connect(&setting, qOverload<SettingsManager::ConstKey, const QVariant&>(&SettingsManager::settingChanged), this,
            [this](const SettingsManager::ConstKey key, const QVariant &val) {
                switch (key) {
                    case SettingsManager::inopEnumItem_constKey:
                    case SettingsManager::spliterSta:
                    case SettingsManager::MainWindowGeo:
                    case SettingsManager::MainWidgetSta:
                    case SettingsManager::stayFront:
                    case SettingsManager::scaleBarEnable:
                    case SettingsManager::dataSource:
                    case SettingsManager::planeFollowed:
                        break;
                    case SettingsManager::OptionWidgetGeo: {
                        restoreGeometry(val.toByteArray());
                        break;
                    }
                    default:
                        assert(false && "need to update switch case. [options_widget::initConnect]");
                }
            });
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
    const int displayFile = settings.value("displayFile", 0).toInt();
    ui->onlyPdf_comboBox->setCurrentIndex(displayFile);
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
    // 文件夹
    settings.setValue("chartFolder", ui->chartFolder_lineEdit->text());
    settings.setValue("mappingFolder", ui->mappingFoler_lineEdit->text());
    settings.setValue("displayFile", ui->onlyPdf_comboBox->currentIndex());
    // TCAS 范围
    settings.setValue("tcasMode", ui->tcas_comboBox->currentIndex());
    settings.setValue("altMode", ui->alt_comboBox->currentIndex());
}
