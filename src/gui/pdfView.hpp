#ifndef CHARTNAVIGATION_PDFVIEW_HPP
#define CHARTNAVIGATION_PDFVIEW_HPP

#include <QtPdf/QtPdf>
#include <QtPdfWidgets/QPdfView>
#include "XPlaneUDP.hpp"
#include "utils/affineTransformer.hpp"

class PdfView final : public QGraphicsView {
        Q_OBJECT
    public:
        explicit PdfView (QWidget *parent = nullptr);
        void loadMappingData (const std::vector<std::vector<double>> &data);
        void setPdf (QPdfDocument *file);
    private:
        // 地图缩放逻辑
        void wheelEvent (QWheelEvent *event) override;
        // 飞机绘制逻辑
        std::pair<double, double> trans ();
        void paintEvent (QPaintEvent *event) override;
        // x-plane逻辑
        void xpInfoUpdate ();

        // 渲染
        QPdfPageRenderer render;
        QGraphicsScene *scene;
        // 仿射变换
        QSizeF docSize{-1, -1};
        AffineTransformer transformer{};
        bool transActive{false};
        // x-plane
        QPixmap plane;
        eyderoe::XPlaneUdp xp;
        eyderoe::XPlaneUdp::PlaneInfo planeInfo{.track = -999};
        bool connected{false};
        QTimer timer;
    private Q_SLOTS:
        void renderComplete (int pageNumber, QSize imageSize, const QImage &image, QPdfDocumentRenderOptions options);
};

#endif //CHARTNAVIGATION_PDFVIEW_HPP
