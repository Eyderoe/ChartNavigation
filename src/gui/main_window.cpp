#include "main_window.hpp"

#include <QFileDialog>
#include <QMenu>
#include <QToolBar>
#include <QToolButton>

#include "main_widget.hpp"
#include "options_widget.hpp"
#include "about_dialog.hpp"
#include "ui_main_window.h"
#include "connector/allAdapter.hpp"
#include "ui/statusBar.hpp"
#include "ui/themeColor.hpp"
#include "ui/pdfView.hpp"
#include "ui/stackedWidget.hpp"
#include "utils/settingManage.hpp"
#include "utils/constValue.hpp"


main_window::main_window (QWidget *parent) : QMainWindow(parent), ui(new Ui::main_window) {
    ui->setupUi(this);
    // 初始化页面
    stackedWidget = new StackedWidget(this);
    pdfBrowser = new main_widget(this);
    enroute = new enroute_widget(this);
    stackedWidget->addWidget(pdfBrowser);
    stackedWidget->addWidget(enroute);
    setCentralWidget(stackedWidget);
    // 初始化动作组
    initActionGroup();
    // 安卓特化 因为显示不了菜单栏
    if constexpr (platform == MultiPlatform::androidOS)
        menu2toolBar();
    // 初始化状态栏
    new StatusBar(ui->statusbar, ui->statusbar);
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
    const auto centralWidget = dynamic_cast<QStackedWidget*>(this->centralWidget());
    const auto pdfWidget = dynamic_cast<main_widget*>(centralWidget->widget(0));
    pdfWidget->saveSplitter();
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
    auto &setting = SettingsManager::instance();
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
                    case SettingsManager::showThumb:
                        ui->action_show_thumb->setChecked(val.toBool());
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
    connect(ui->action_show_thumb, &QAction::triggered, this, [&](const bool checked) {
        SettingsManager::instance().set(SettingsManager::showThumb, checked);
    });
    connect(ui->action_scale, &QAction::triggered, this, [&](const bool checked) {
        SettingsManager::instance().set(SettingsManager::scaleBarEnable, checked);
    });
    connect(ui->action_top, &QAction::triggered, this, [&](const bool checked) {
        SettingsManager::instance().set(SettingsManager::stayFront, checked);
    });
    connect(ui->action_follow, &QAction::triggered, this, [&](const bool checked) {
        SettingsManager::instance().set(SettingsManager::planeFollowed, checked);
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
    connect(tcasGroup, &QActionGroup::triggered, this, [&](const QAction *action) {
        if (action == ui->action_tcas_all)
            SettingsManager::instance().set(SettingsManager::tcasRange, static_cast<int>(TcasMode::all));
        else if (action == ui->action_tcas_none)
            SettingsManager::instance().set(SettingsManager::tcasRange, static_cast<int>(TcasMode::none));
        else if (action == ui->action_tcas_nm30)
            SettingsManager::instance().set(SettingsManager::tcasRange, static_cast<int>(TcasMode::nm30));
        else if (action == ui->action_tcas_nm6)
            SettingsManager::instance().set(SettingsManager::tcasRange, static_cast<int>(TcasMode::nm6));
        else
            assert(false && "need to update if else. [main_window::initConnect]");
    });
    connect(altGroup, &QActionGroup::triggered, this, [&](const QAction *action) {
        if (action == ui->action_alt_none)
            SettingsManager::instance().set(SettingsManager::altMode, static_cast<int>(AltMode::none));
        else if (action == ui->action_alt_feet)
            SettingsManager::instance().set(SettingsManager::altMode, static_cast<int>(AltMode::feet));
        else if (action == ui->action_alt_meter)
            SettingsManager::instance().set(SettingsManager::altMode, static_cast<int>(AltMode::meter));
        else
            assert(false && "need to update if else. [main_window::initConnect]");
    });
    connect(ui->action_load_file, &QAction::triggered, this, &main_window::openFile);
    connect(ui->action_load_folder, &QAction::triggered, this, &main_window::openFolder);
    connect(ui->action_switch, &QAction::triggered, this,
            [this] () { stackedWidget->setCurrentIndex(mainPage.next()); });
    connect(ui->action_rotate, &QAction::triggered, this,
            [&] () { setting.set(SettingsManager::pageRotate, pageRotateDegree.next()); });
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

/**
 * @brief 把菜单栏转换为工具栏
 */
void main_window::menu2toolBar () {
    auto *bar = ui->toolBar;
    bar->addSeparator();
    const auto addTopMenu = [bar](QMenu *menu) {
        auto *btn = new QToolButton(bar);
        btn->setText(menu->title());
        btn->setMenu(menu);
        btn->setPopupMode(QToolButton::InstantPopup);
        btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
        btn->setFocusPolicy(Qt::NoFocus);
        bar->addWidget(btn);
    };
    auto menus = ui->menubar->findChildren<QMenu*>(QRegularExpression("^menu_top_*"));
    std::ranges::sort(menus, std::ranges::less{}, &QMenu::objectName);
    for (const auto menu : menus)
        addTopMenu(menu);
    ui->menubar->hide();
}

void main_window::on_action_dark_triggered (const bool checked) {
    SettingsManager::instance().set(SettingsManager::isDarkTheme, checked);
}

void main_window::openFile () {
    // 文件选择框的一坨
    auto option = QFileDialog::Options();
    if ((platform == MultiPlatform::macOS) && !inMacSandbox)
        option |= QFileDialog::DontUseNativeDialog;
    const QString fileName = QFileDialog::getOpenFileName(this, "选择文件", QDir::homePath()
                                                          , "文件 (*.pdf)", nullptr, option);
    if (fileName.isEmpty())
        return;
    // 读取文件
    pdfBrowser->loadPdfFile(fileName);
}

void main_window::openFolder () {
    // 文件选择框的一坨
    auto option = QFileDialog::Options();
    if ((platform == MultiPlatform::macOS) && !inMacSandbox)
        option |= QFileDialog::DontUseNativeDialog;
    option = option | QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks;
    const QString dir = QFileDialog::getExistingDirectory(this, "选择文件夹", QDir::homePath()
                                                          , option);
    if (dir.isEmpty())
        return;
    // 读取文件夹
    pdfBrowser->loadFolder(dir);
}
