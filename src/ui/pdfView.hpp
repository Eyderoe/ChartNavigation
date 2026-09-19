#ifndef CHARTNAVIGATION_PDFVIEW_HPP
#define CHARTNAVIGATION_PDFVIEW_HPP

#include <QtPdfWidgets/QPdfView>
#include <QPointer>
#include <optional>

#include "services/attachedChart.hpp"
#include "utils/affineTransformer.hpp"
#include "services/dataProvider.hpp"

// https://doc-snapshots.qt.io/qt6-6.9/qtpdf-index.html
class PdfView final : public QPdfView {
        Q_OBJECT
    public:
        explicit PdfView (QWidget *parent = nullptr);
        void setCenterOn (bool center);
        void centerOwnAircraft ();
        void setColorTheme (bool darkTheme);
        void loadMappingData (const std::vector<std::vector<double>> &data, double rotateDegree, double threshold);
        void closeSimulation () const;
        void setDataProvider (DataProvider *provider);
        void fetchScale ();
        void zoomTo (double factor);
        [[nodiscard]] bool canAttachCurrentPage () const;
        [[nodiscard]] std::optional<AttachedChart> currentPageAttachment ();
    protected:
        void wheelEvent (QWheelEvent *event) override;
        void mousePressEvent (QMouseEvent *event) override;
        void mouseMoveEvent (QMouseEvent *event) override;
        void mouseReleaseEvent (QMouseEvent *event) override;
        void paintEvent (QPaintEvent *event) override;
    private:
        void initConnect ();
        void freezeViewport (double targetZoom);
        void clearFrozenViewport ();
        // 模拟器部分
        std::pair<double, double> trans (const Point2D &position);
        std::pair<double, double> trans (double latitude, double longitude);
        void drawPlane (QPainter &painter, int idx = 0);
        void onDataUpdated ();
        // 杂
        [[nodiscard]] QSizeF getDocSize () const;

        // 地图拖动逻辑
        bool dragging{};
        QPoint lastPos{};
        // PDF 缩放时保留旧帧，直到目标分辨率的页面完成渲染
        QPixmap frozenViewport{};
        int frozenPage{-1};
        QSize targetRenderSize{};
        bool renderCompletionTracked{};
        // 地图显示逻辑
        bool centerOn{};
        bool isDark{};
        double rotate{}; // 地图映射文件得到，旋转灰机
        // 仿射变换
        AffineTransformer transformer{};
        bool transActive{false};
        AffineQuality affineQuality{AffineQuality::inop};
        // 模拟器
        QPointer<DataProvider> dataProvider;
        QPixmap plane, otherPlane;
    Q_SIGNALS:
        void zoomFactor_changed (double factor);
};


constexpr double zoomMin{0.2};
constexpr double zoomMax{4};

#endif //CHARTNAVIGATION_PDFVIEW_HPP
