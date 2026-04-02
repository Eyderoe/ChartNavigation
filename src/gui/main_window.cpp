#include "main_window.hpp"

#include "main_widget.hpp"
#include "ui_main_window.h"
#include "ui/themeColor.hpp"
#include "utils/settingManage.hpp"
#include "about_dialog.hpp"
#include "connector/allAdapter.hpp"


main_window::main_window (QWidget *parent) : QMainWindow(parent), ui(new Ui::main_window) {
    ui->setupUi(this);
    setCentralWidget(new main_widget(this));
    // 连接信号
    initConnect();
    // 初始化动作组
    initAction();
    // 更新所有设置
    SettingsManager &ins = SettingsManager::instance();
    ins.broadcast();

    restoreGeometry(ins.get(SettingsManager::MainWindowGeo, {}).toByteArray()); // 窗口尺寸
    const bool isDark = QApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark; // 暗色模式以及按钮
    ins.set(SettingsManager::isDarkTheme, isDark);
}

main_window::~main_window () {
    delete ui;
}

/**
 * @brief 设置色彩主题
 * @param colorScheme 色彩主题 0u 1light 2dark
 */
void main_window::setTheme (const Qt::ColorScheme colorScheme) {
    if (colorScheme == Qt::ColorScheme::Dark) {
        setDarkTheme();
    } else {
        setLightTheme();
    }
}

void main_window::closeEvent (QCloseEvent *event) {
    SettingsManager &manager = SettingsManager::instance();
    manager.set(SettingsManager::MainWindowGeo, "");
    manager.writeSetting();
    QMainWindow::closeEvent(event);
}

void main_window::initConnect () {
    auto &setting = SettingsManager::instance();
    // 存储设置
    connect(&setting, qOverload<SettingsManager::ConstKey, const QVariant&>(&SettingsManager::settingChanged), this,
            [this](const SettingsManager::ConstKey key, const QVariant &val) {
                switch (key) {
                    case SettingsManager::inopEnumItem_constKey: break;
                    case SettingsManager::MainWindowGeo: break;
                    case SettingsManager::dataSource: {
                        switch (static_cast<SimulatorSource>(val.toInt())) {
                            case SimulatorSource::xplane:
                                ui->action_source_XPlane->setChecked(true);
                                break;
                            case SimulatorSource::wlan:
                                ui->action_source_wlan->setChecked(true);
                                break;
                            case SimulatorSource::real:
                                ui->action_source_real->setChecked(true);
                                break;
                            default:
                                assert(false && "need to update switch case. [main_window::initConnect]");
                        }
                        break;
                    }
                    default:
                        assert(false && "need to update switch case. [main_window::initConnect]");
                }
            });
    // 临时设置
    connect(&setting, qOverload<SettingsManager::TempKey, const QVariant&>(&SettingsManager::settingChanged), this,
            [this](const SettingsManager::TempKey key, const QVariant &val) {
                switch (key) {
                    case SettingsManager::isDarkTheme: {
                        const auto isDark = val.toBool();
                        setTheme(isDark ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light);
                        ui->action_dark->setChecked(isDark);
                        break;
                    }
                    default:
                        assert(false && "need to update switch case. [main_window::initConnect]");
                }
            });
    // QMainWindow动作
    connect(ui->action_thank, &QAction::triggered, this, [&] () {
        const auto dialog = new about_dialog(this);
        dialog->show();
    });
    connect(sourceGroup, &QActionGroup::triggered, this, [&](const QAction *action) {
        if (action == ui->action_source_XPlane)
            setting.set(SettingsManager::dataSource, static_cast<int>(SimulatorSource::xplane));
        else if (action == ui->action_source_wlan)
            setting.set(SettingsManager::dataSource, static_cast<int>(SimulatorSource::wlan));
        else if (action == ui->action_source_real)
            setting.set(SettingsManager::dataSource, static_cast<int>(SimulatorSource::real));
        else
            assert(false && "need to update if else. [main_window::initConnect]");
    });
}

void main_window::initAction () {
    sourceGroup = new QActionGroup(this);
    sourceGroup->setExclusive(true);
    sourceGroup->addAction(ui->action_source_XPlane);
    sourceGroup->addAction(ui->action_source_wlan);
    sourceGroup->addAction(ui->action_source_real);
}

void main_window::on_action_dark_triggered (const bool checked) {
    SettingsManager::instance().set(SettingsManager::isDarkTheme, checked);
}
