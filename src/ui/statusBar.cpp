#include "statusBar.hpp"
#include <QString>

StatusBar::StatusBar (QStatusBar *bar, QObject *parent) : QObject(parent) {
    // 状态栏基本外观
    auto addSeparator = [bar] () {
        auto *line = new QFrame(bar);
        line->setFrameShape(QFrame::VLine);
        line->setFrameShadow(QFrame::Sunken);
        line->setFixedWidth(2);
        line->setStyleSheet("color: gray; background-color: gray;");
        bar->addWidget(line);
    };
    this->bar = bar;
    simuLabel = new QLabel("- 离线");
    bar->addWidget(simuLabel);
    addSeparator();
    planeLabel = new QLabel("(-, -) AGL:-ft");
    bar->addWidget(planeLabel);
    addSeparator();
    affineLabel = new QLabel("误差:- 质量:-");
    bar->addWidget(affineLabel);
    // 定时器
    timer.setInterval(1000);
    connect(&timer, &QTimer::timeout, this, &StatusBar::update);
    timer.start();
    // 设置初始化
    const SettingsManager &setting = SettingsManager::instance();
    connect(&setting, qOverload<SettingsManager::ConstKey, const QVariant&>(&SettingsManager::settingChanged), this,
            [this](const SettingsManager::ConstKey key, const QVariant &val) {
                switch (key) {
                    case SettingsManager::dataSource: {
                        simu.first = static_cast<SimulatorSource>(val.toInt());
                        hasUpdate = true;
                        break;
                    }
                    default:
                        break;
                }
            });
    // 临时设置
    connect(&setting, qOverload<SettingsManager::TempKey, const QVariant&>(&SettingsManager::settingChanged), this,
            [this](const SettingsManager::TempKey key, const QVariant &val) {
                switch (key) {
                    case SettingsManager::affineError:
                        affine.first = val.toDouble();
                        hasUpdate = true;
                        break;
                    case SettingsManager::affineQuality:
                        affine.second = static_cast<AffineQuality>(val.toInt());
                        hasUpdate = true;
                        break;
                    default:
                        break;
                }
            });
}

void StatusBar::update () {
    if (!hasUpdate)
        return;
    hasUpdate = false;
    // 模拟器
    // 信息
    // 仿射变换 [误差:- 质量:-]
    if (std::isnan(affine.first)) {
        affineLabel->setText("误差:- 质量:-");
    } else {
        QString quality;
        switch (affine.second) {
            case AffineQuality::inop:
                break;
            case AffineQuality::bad:
                quality = "差";
                break;
            case AffineQuality::hmmm:
                quality = "中";
                break;
            case AffineQuality::good:
                quality = "好";
                break;
        }
        affineLabel->setText(QString("误差:%1 质量:%2").arg(affine.first, 0, 'f', 1).arg(quality));
    }
}
