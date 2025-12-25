#ifndef CHARTNAVIGATION_PDFVIEW_HPP
#define CHARTNAVIGATION_PDFVIEW_HPP

#include <QtPdf/QtPdf>
#include <QtPdfWidgets/QPdfView>
#include "XPlaneUDP.hpp"
#include "utils/affineTransformer.hpp"

// https://doc-snapshots.qt.io/qt6-6.9/qtpdf-index.html
class PdfView final : public QPdfView {
    public:
        explicit PdfView (QWidget *parent = nullptr);
        QSizeF getDocSize(int page=0) const;
        void setCenterOn (bool center);
        void setColorTheme(bool darkTheme);
        void loadMappingData (const std::vector<std::vector<double>> &data);
    private:
        // 地图缩放逻辑
        void wheelEvent (QWheelEvent *event) override;
        // 地图拖动逻辑
        void mousePressEvent (QMouseEvent *event) override;
        void mouseMoveEvent (QMouseEvent *event) override;
        void mouseReleaseEvent (QMouseEvent *event) override;
        // 地图绘制逻辑
        std::pair<double,double> trans(double latitude, double longitude);
        void paintEvent (QPaintEvent *event) override;
        // x-plane逻辑
        void xpInfoUpdate ();

        // 地图拖动逻辑
        bool dragging{};
        QPoint lastPos{};
        // 地图显示逻辑
        bool centerOn{};
        bool isDark{};
        // 仿射变换
        AffineTransformer transformer{};
        bool transActive{false};
        // x-plane
        QPixmap plane;
        eyderoe::XPlaneUdp xp;
        eyderoe::XPlaneUdp::PlaneInfo planeInfo{.track = -999};
        bool connected{false};
        // 定时器
        QTimer xpUpdateTimer;
};

#endif //CHARTNAVIGATION_PDFVIEW_HPP
