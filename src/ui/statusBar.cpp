#include "statusBar.hpp"
#include <QString>

#include "utils/constValue.hpp"

StatusBar::StatusBar (QStatusBar *bar, QObject *parent) : QObject(parent) {
    // 状态栏基本外观
    auto addSeparator = [bar] () {
        auto *line = new QFrame(bar);
        line->setFrameShape(QFrame::VLine);
        line->setFrameShadow(QFrame::Sunken);
        line->setFixedWidth(1);
        line->setStyleSheet("color: gray; background-color: gray;");
        bar->addWidget(line);
    };
    this->bar = bar;
    simuLabel = new QLabel("- 离线");
    bar->addWidget(simuLabel);
    addSeparator();
    if constexpr (platform != MultiPlatform::androidOS)
        planeLabel = new QLabel("(-,-) AGL:-ft");
    else
        planeLabel = new QLabel("(-,-) Alt:-ft");
    bar->addWidget(planeLabel);
    addSeparator();
    affineLabel = new QLabel("误差:- 质量:-");
    bar->addWidget(affineLabel);
    affine.first = NaN;
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
                        updateSimu = true;
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
                        updateAffine = true;
                        break;
                    case SettingsManager::affineQuality:
                        affine.second = static_cast<AffineQuality>(val.toInt());
                        updateAffine = true;
                        break;
                    case SettingsManager::simuConnect:
                        simu.second = val.toBool();
                        updateSimu = true;
                        updatePlane = true;
                        break;
                    case SettingsManager::latitu:
                        plane.first.first = val.toDouble();
                        updatePlane = true;
                        break;
                    case SettingsManager::longitu:
                        plane.first.second = val.toDouble();
                        updatePlane = true;
                        break;
                    case SettingsManager::altRelat:
                        plane.second = val.toInt();
                        updatePlane = true;
                        break;
                    default:
                        break;
                }
            });
}

void StatusBar::update () {
    if (!updateSimu && !updatePlane && !updateAffine)
        return;
    // 模拟器
    if (updateSimu) {
        updateSimu = false;
        QString simuStr;
        switch (simu.first) {
            case SimulatorSource::wlan:
                simuStr = "局域网";
                break;
            case SimulatorSource::real:
                simuStr = "现实";
                break;
            case SimulatorSource::xplane:
            default:
                simuStr = "XPlane";
                break;
        }
        simuStr += simu.second ? " 在线" : " 离线";
        simuLabel->setText(simuStr);
    }
    // 信息
    if (updatePlane) {
        updatePlane = false;
        std::string infoText;
        auto &[lat, lon] = plane.first;
        int altRela = plane.second;
        if constexpr (platform != MultiPlatform::androidOS) {
            if (simu.second) {
                infoText = std::format("({:.3f}, {:.3f}) AGL:{}ft"
                                       , lat, lon, altRela == -500 ? '-' : altRela);
            } else
                infoText = "(-,-) AGL:-ft";
        } else {
            if (simu.second) {
                infoText = std::format("({:.3f}, {:.3f}) Alt:{}ft", lat, lon, altRela);
            } else
                infoText = "(-,-) Alt:-ft";
        }
        planeLabel->setText(QString::fromStdString(infoText));
    }
    // 仿射变换 [误差:- 质量:-]
    if (updateAffine) {
        updateAffine = false;
        if (std::isnan(affine.first)) {
            affineLabel->setText("误差:- 质量:-");
        } else {
            QString quality;
            switch (affine.second) {
                case AffineQuality::bad:
                    quality = "差";
                    break;
                case AffineQuality::fine:
                    quality = "中";
                    break;
                case AffineQuality::good:
                    quality = "好";
                    break;
                case AffineQuality::inop:
                default:
                    break;
            }
            affineLabel->setText(QString("误差:%1 质量:%2").arg(affine.first, 0, 'f', 1).arg(quality));
        }
    }
}
