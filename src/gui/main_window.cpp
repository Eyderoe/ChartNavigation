#include "main_window.hpp"

#include <QFileDialog>

#include "main_widget.hpp"
#include "ui_main_window.h"
#include "options_widget.hpp"
#include "ui/themeColor.hpp"
#include "utils/settingManage.hpp"
#include "about_dialog.hpp"
#include "connector/allAdapter.hpp"
#include "utils/constValue.hpp"
#include "ui/pdfView.hpp"


main_window::main_window (QWidget *parent) : QMainWindow(parent), ui(new Ui::main_window) {
    ui->setupUi(this);
    setCentralWidget(new main_widget(this));
    // 初始化动作组
    initActionGroup();
    // 连接信号
    initConnect();
    // 更新所有设置
    SettingsManager &ins = SettingsManager::instance();
    ins.broadcast();

    const bool isDark = QApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark; // 暗色模式以及按钮
    ins.set(SettingsManager::isDarkTheme, isDark);
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
    const auto centralWidget = dynamic_cast<main_widget*>(this->centralWidget());
    centralWidget->saveSplitter();
    SettingsManager &manager = SettingsManager::instance();
    manager.set(SettingsManager::MainWindowGeo, saveGeometry(), true);
    manager.set(SettingsManager::MainWidgetSta, saveState(), true);

    manager.writeSetting();
    QMainWindow::closeEvent(event);
}

void main_window::setDataSourceGroup (int val) const {
    switch (static_cast<SimulatorSource>(val)) {
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
            assert(false && "need to update switch case. [main_window::setDataSource]");
    }
}

void main_window::setTcasRangeGroup (int val) const {
    switch (static_cast<TcasMode>(val)) {
        case TcasMode::nm30:
            ui->action_tcas_nm30->setChecked(true);
            break;
        case TcasMode::nm6:
            ui->action_tcas_nm6->setChecked(true);
            break;
        case TcasMode::none:
            ui->action_tcas_none->setChecked(true);
            break;
        case TcasMode::all:
            ui->action_tcas_all->setChecked(true);
            break;
        default:
            assert(false && "need to update switch case. [main_window::setTcasRange]");
    }
}

void main_window::setAltModeGroup (int val) const {
    switch (static_cast<AltMode>(val)) {
        case AltMode::none:
            ui->action_alt_none->setChecked(true);
            break;
        case AltMode::feet:
            ui->action_alt_feet->setChecked(true);
            break;
        case AltMode::meter:
            ui->action_alt_meter->setChecked(true);
            break;
        default:
            assert(false && "need to update switch case. [main_window::setAltModeGroup]");
    }
}

void main_window::initConnect () {
    const auto &setting = SettingsManager::instance();
    // 存储设置
    connect(&setting, qOverload<SettingsManager::ConstKey, const QVariant&>(&SettingsManager::settingChanged), this,
            [this](const SettingsManager::ConstKey key, const QVariant &val) {
                switch (key) {
                    case SettingsManager::MainWindowGeo:
                        restoreGeometry(val.toByteArray());
                        break;
                    case SettingsManager::MainWidgetSta:
                        restoreState(val.toByteArray());
                        break;
                    case SettingsManager::dataSource:
                        setDataSourceGroup(val.toInt());
                        break;
                    case SettingsManager::tcasRange:
                        setTcasRangeGroup(val.toInt());
                        break;
                    case SettingsManager::altMode:
                        setAltModeGroup(val.toInt());
                        break;
                    case SettingsManager::planeFollowed:
                        ui->action_follow->setChecked(val.toBool());
                        break;
                    case SettingsManager::stayFront:
                        if (val.toBool())
                            setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
                        else
                            setWindowFlags(windowFlags() & ~Qt::WindowStaysOnTopHint);
                        ui->action_top->setChecked(val.toBool());
                        show();
                        break;
                    case SettingsManager::scaleBarEnable:
                        ui->action_scale->setChecked(val.toBool());
                        break;
                    default:
                        break;
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
                        break;
                }
            });
    // QMainWindow动作
    connect(ui->action_thank, &QAction::triggered, this, [&] () {
        const auto dialog = new about_dialog(this);
        dialog->show();
    });
    connect(ui->action_setting, &QAction::triggered, this, [&] () {
        const auto options = new options_widget(this);
        options->setWindowFlags(Qt::Window);
        options->show();
        options->setAttribute(Qt::WA_DeleteOnClose);
    });
    connect(ui->action_dark, &QAction::triggered, this, [&](const bool checked) {
        SettingsManager::instance().set(SettingsManager::isDarkTheme, checked);
    });
    connect(ui->action_scale, &QAction::triggered, this, [&](const bool checked) {
        SettingsManager::instance().set(SettingsManager::scaleBarEnable, checked);
    });
    connect(ui->action_top, &QAction::triggered, this, [&](const bool checked) {
        SettingsManager::instance().set(SettingsManager::stayFront, checked);
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

/**
 * @brief 创建一个动作组
 * @param widget 窗口
 * @param contain 动作包含的名字
 * @return 动作组指针
 */
QActionGroup* makeGroup (QWidget *widget, const QString &contain) {
    const auto group = new QActionGroup(widget);
    group->setExclusive(true);
    for (QAction *action : widget->findChildren<QAction*>()) {
        if (action->objectName().contains(contain))
            group->addAction(action);
    }
    return group;
}

void main_window::initActionGroup () {
    sourceGroup = makeGroup(this, "_source_");
    tcasGroup = makeGroup(this, "_tcas_");
    altGroup = makeGroup(this, "_alt_");
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
    if (dir.isEmpty())
        return;
    // 读取文件夹
    const auto widget = dynamic_cast<main_widget*>(centralWidget());
    widget->loadFolder(dir);
}
