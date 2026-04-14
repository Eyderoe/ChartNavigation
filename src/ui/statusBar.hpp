#ifndef CHARTNAVIGATION_STATUSBAR_HPP
#define CHARTNAVIGATION_STATUSBAR_HPP

#include "connector/allAdapter.hpp"
#include "utils/settingManage.hpp"
#include "utils/geographic.hpp"
#include "utils/affineTransformer.hpp"

class StatusBar : public QObject {
        Q_OBJECT
    public:
        explicit StatusBar (QStatusBar *bar, QObject *parent = nullptr);
    private:
        QStatusBar *bar;
        QLabel *simuLabel, *planeLabel, *affineLabel;
        QTimer timer;
        std::pair<SimulatorSource, bool> simu; // 模拟器
        std::pair<Point2D, int> plane; // 信息
        std::pair<double, AffineQuality> affine; // 仿射变换
        bool hasUpdate{false};

        void update();
};

#endif //CHARTNAVIGATION_STATUSBAR_HPP
