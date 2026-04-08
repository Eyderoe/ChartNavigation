#include "main_window.hpp"

#include <QFileDialog>

#include "main_widget.hpp"
#include "ui_main_window.h"
#include "ui/themeColor.hpp"
#include "utils/settingManage.hpp"
#include "about_dialog.hpp"
#include "connector/allAdapter.hpp"
#include "utils/constValue.hpp"


main_window::main_window (QWidget *parent) : QMainWindow(parent), ui(new Ui::main_window) {
    ui->setupUi(this);
    setCentralWidget(new main_widget(this));
    // 初始化动作组
    initAction();
    // 连接信号
    initConnect();
    // 更新所有设置
    SettingsManager &ins = SettingsManager::instance();
    ins.broadcast();

    const bool isDark = QApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark; // 暗色模式以及按钮
    ins.set(SettingsManager::isDarkTheme, isDark);
}

main_window::~main_window () {
    SettingsManager &ins = SettingsManager::instance();
    ins.set(SettingsManager::MainWindowGeo, saveGeometry());
    ins.set(SettingsManager::MainWidgetSta, saveState());

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
                    case SettingsManager::inopEnumItem_constKey:
                    case SettingsManager::spliterSta:
                        break;
                    case SettingsManager::MainWindowGeo: {
                        restoreGeometry(val.toByteArray());
                        break;
                    }
                    case SettingsManager::MainWidgetSta: {
                        restoreState(val.toByteArray());
                        break;
                    }
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
                    case SettingsManager::planeFollowed: {
                        ui->action_follow->setChecked(val.toBool());
                        break;
                    }
                    case SettingsManager::stayFront: {
                        if (val.toBool())
                            setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
                        else
                            setWindowFlags(windowFlags() & ~Qt::WindowStaysOnTopHint);
                        ui->action_top->setChecked(val.toBool());
                        show();
                        break;
                    }
                    case SettingsManager::scaleBarEnable: {
                        ui->action_scale->setChecked(val.toBool());
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
    connect(ui->action_dark, &QAction::triggered, this, [&](const bool checked) {
        SettingsManager::instance().set(SettingsManager::isDarkTheme, checked);
    });
    connect(ui->action_scale, &QAction::triggered, this, [&](const bool checked) {
        SettingsManager::instance().set(SettingsManager::scaleBarEnable, checked);
    });
    connect(sourceGroup, &QActionGroup::triggered, this, [&](const QAction *action) {
        if (action == ui->action_source_XPlane)
            SettingsManager::instance().set(SettingsManager::dataSource, static_cast<int>(SimulatorSource::xplane));
        else if (action == ui->action_source_wlan)
            SettingsManager::instance().set(SettingsManager::dataSource, static_cast<int>(SimulatorSource::wlan));
        else if (action == ui->action_source_real)
            SettingsManager::instance().set(SettingsManager::dataSource, static_cast<int>(SimulatorSource::real));
        else
            assert(false && "need to update if else. [main_window::initConnect]");
    });
    connect(ui->action_load_file, &QAction::triggered, this, &main_window::openFile);
    connect(ui->action_load_folder, &QAction::triggered, this, &main_window::openFolder);
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

void main_window::openFile () {
    // 文件选择框的一坨
    auto option = QFileDialog::Options();
    if ((platform == MultiPlatform::mac) && !inMacSandbox)
        option |= QFileDialog::DontUseNativeDialog;
    const QString fileName = QFileDialog::getOpenFileName(this, "选择文件", QDir::homePath()
                                                          , "文件 (*.pdf)", nullptr, option);
    if (fileName.isEmpty())
        return;
    // 读取文件
    const auto widget = dynamic_cast<main_widget*>(centralWidget());
    widget->loadPdfFile(fileName);
}

void main_window::openFolder () {
    // 文件选择框的一坨
    auto option = QFileDialog::Options();
    if ((platform == MultiPlatform::mac) && !inMacSandbox)
        option |= QFileDialog::DontUseNativeDialog;
    option = option | QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks;
    const QString dir = QFileDialog::getExistingDirectory(this, "选择文件夹", QDir::homePath()
                                                          , option);
    if (!dir.isEmpty())
        return;
    // 读取文件夹
    const auto widget = dynamic_cast<main_widget*>(centralWidget());
    qDebug() << "没写完 [main_window::openFolder]";
}
