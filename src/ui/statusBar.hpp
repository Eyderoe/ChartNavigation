#ifndef CHARTNAVIGATION_STATUSBAR_HPP
#define CHARTNAVIGATION_STATUSBAR_HPP

#include <QPointer>

#include "connector/allAdapter.hpp"
#include "services/settingManage.hpp"
#include "utils/geographic.hpp"
#include "utils/affineTransformer.hpp"
#include "services/positionDevice.hpp"

class PdfView;

class StatusBar : public QObject {
        Q_OBJECT
    public:
        explicit StatusBar (QStatusBar *bar, PdfView *pdfView, QObject *parent = nullptr);
    protected:
        bool eventFilter (QObject *watched, QEvent *event) override;
    private:
        QStatusBar *bar;
        QLabel *simuLabel, *planeLabel, *affineLabel, *errorLabel;
        QTimer timer;
        std::pair<SimulatorSource, bool> simu; // 模拟器
        std::pair<Point2D, int> plane; // 信息
        std::pair<double, AffineQuality> affine; // 仿射变换
        std::unique_ptr<PositionDevice> device{nullptr};
        bool updateSimu{false}, updatePlane{false}, updateAffine{false};
        QPointer<PdfView> cursorView;
        QTimer cursorTimer;
        QLabel *cursorLabel{nullptr};
        QFrame *cursorSeparator{nullptr};

        void initCursorCoordinates (PdfView *pdfView);
        void updateCursorCoordinates ();
        void update ();
};

#endif //CHARTNAVIGATION_STATUSBAR_HPP
