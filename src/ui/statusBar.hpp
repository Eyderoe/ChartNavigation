#ifndef CHARTNAVIGATION_STATUSBAR_HPP
#define CHARTNAVIGATION_STATUSBAR_HPP

#include "connector/allAdapter.hpp"
#include "services/settingManage.hpp"
#include "utils/geographic.hpp"
#include "utils/affineTransformer.hpp"
#include "services/positionDevice.hpp"

class StatusBar : public QObject {
        Q_OBJECT
    public:
        explicit StatusBar (QStatusBar *bar, QObject *parent = nullptr);
    private:
        QStatusBar *bar;
        QLabel *simuLabel, *planeLabel, *affineLabel, *errorLabel;
        QTimer timer;
        std::pair<SimulatorSource, bool> simu; // 模拟器
        std::pair<Point2D, int> plane; // 信息
        std::pair<double, AffineQuality> affine; // 仿射变换
        std::unique_ptr<PositionDevice> device{nullptr};
        bool updateSimu{false}, updatePlane{false}, updateAffine{false};

        void update ();
};

#endif //CHARTNAVIGATION_STATUSBAR_HPP
