#ifndef CHARTNAVIGATION_PDFVIEW_HPP
#define CHARTNAVIGATION_PDFVIEW_HPP

#include <QtPdfWidgets/QPdfView>
#include "XPlaneUDP.hpp"
#include "utils/affineTransformer.hpp"

// https://doc-snapshots.qt.io/qt6-6.9/qtpdf-index.html
class PdfView final : public QPdfView {
    public:
        explicit PdfView (QWidget *parent = nullptr);
        QSizeF getDocSize (int page = 0) const;
        void setCenterOn (bool center);
        void setColorTheme (bool darkTheme);
        void loadMappingData (const std::vector<std::vector<double>> &data, double rotateDegree, double threshold);
        void closeXp();
    private:
        // 重载事件部分
        void wheelEvent (QWheelEvent *event) override;
        void mousePressEvent (QMouseEvent *event) override;
        void mouseMoveEvent (QMouseEvent *event) override;
        void mouseReleaseEvent (QMouseEvent *event) override;
        void paintEvent (QPaintEvent *event) override;
        // x-plane部分
        std::pair<double, double> trans (double latitude, double longitude);
        void drawPlane (QPainter &painter,int idx = 0);
        void xpInfoUpdate ();
        void xpInit ();

        // 地图拖动逻辑
        bool dragging{};
        QPoint lastPos{};
        // 地图显示逻辑
        bool centerOn{};
        bool isDark{};
        double rotate{};
        // 仿射变换
        AffineTransformer transformer{};
        bool transActive{false};
        // x-plane
        QPixmap plane, otherPlane;
        eyderoe::XPlaneUdp xp;
        eyderoe::XPlaneUdp::DatarefIndex multiId{}, multiLat{}, multiLon{}, multiAlt{}, multiTrk{}, multiVs{},
                                         multiFlightId{};
        std::array<float, 64> multiIdVal{}, multiLatVal{}, multiLonVal{}, multiAltVal{}, multiTrkVal{}, multiVsVal{};
        std::array<float, 512> multiFlightIdVal{};
        bool connected{false};
        // 定时器
        QTimer xpUpdateTimer;
};

#endif //CHARTNAVIGATION_PDFVIEW_HPP
