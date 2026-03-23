#include "main_window.hpp"

#include "main_widget.hpp"
#include "ui_main_window.h"
#include "gui/themeColor.hpp"
#include "utils/settingManage.hpp"


main_window::main_window (QWidget *parent) : QMainWindow(parent), ui(new Ui::main_window) {
    ui->setupUi(this);
    // 连接信号
    initConnect();
    // 设置其他的
    setCentralWidget(new main_widget(this));
    // 更新所有设置
    SettingsManager::instance().broadcast();
    restoreGeometry(SettingsManager::instance().get(SettingsManager::MainWindowGeo, {}).toByteArray()); // 窗口尺寸
    const bool isDark = QApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark; // 暗色模式以及按钮
    SettingsManager::instance().set(SettingsManager::isDarkTheme, isDark);
    ui->action_dark->setChecked(isDark);
}

main_window::~main_window () {
    delete ui;
}

/**
 * @brief 设置色彩主题
 * @param colorScheme 色彩主题 0u 1light 2dark
 */
void main_window::setTheme (const Qt::ColorScheme colorScheme) const {
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
    // 设置
    const auto &setting = SettingsManager::instance();
    connect(&setting, &SettingsManager::settingChanged, this,
            [this](const SettingsManager::ConfigKey key, const QVariant &val) {
                switch (key) {
                    case SettingsManager::inopEnumItem: break;
                    case SettingsManager::MainWindowGeo: break;
                    case SettingsManager::isDarkTheme: {
                        const auto isDark = val.toBool();
                        setTheme(isDark ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light);
                        break;
                    }
                    default:
                        assert(false && "need to update switch case. [main_window::initConnect]");
                }
            });
}

void main_window::on_action_dark_triggered (const bool checked) {
    SettingsManager::instance().set(SettingsManager::isDarkTheme, checked);
}
