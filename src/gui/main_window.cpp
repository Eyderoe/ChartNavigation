#include "main_window.hpp"

#include "main_widget.hpp"
#include "ui_main_window.h"
#include "gui/themeColor.hpp"
#include "utils/settingManage.hpp"


main_window::main_window (QWidget *parent) : QMainWindow(parent), ui(new Ui::main_window) {
    ui->setupUi(this);
    // 连接信号
    initSetting();
    // 设置其他的
    setCentralWidget(new main_widget(this));
    // 更新所有设置
    SettingsManager::instance().broadcast();
}

main_window::~main_window () {
    delete ui;
}

/**
 * @brief 设置色彩主题
 * @param colorScheme 色彩主题
 */
void main_window::setTheme (const Qt::ColorScheme colorScheme) const {
    if (colorScheme == Qt::ColorScheme::Dark) {
        setDarkTheme();
    } else {
        setLightTheme();
    }
}

void main_window::initSetting () {
    const auto &setting = SettingsManager::instance();
    connect(&setting, &SettingsManager::settingChanged, this,
            [this](const SettingsManager::ConfigKey key, const QVariant &val) {
                switch (key) {
                    case SettingsManager::inopEnumItem: break;
                    case SettingsManager::MainWindowGeo: break;
                    case SettingsManager::isDarkTheme:
                        auto idx = val.toInt();
                        setTheme(static_cast<Qt::ColorScheme>(idx));
                        break;
                }
            });
}
