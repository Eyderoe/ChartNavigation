#ifndef CHARTNAVIGATION_ENHANCEDMAP_HPP
#define CHARTNAVIGATION_ENHANCEDMAP_HPP

#include <QGraphicsView>
#include <QPixmap>
#include <QPointer>

#include <memory>
#include <optional>
#include <vector>

#include "services/attachedChart.hpp"
#include "utils/geographic.hpp"


class MapItemManage;
class DataProvider;
class QGraphicsScene;
class QGraphicsPixmapItem;
class QFrame;
class QPainter;
class QMouseEvent;
class QResizeEvent;
class QToolButton;
class QTextEdit;

enum class MapZoomLevel : int { nm25, nm50, nm100, nm200 };

class MapView : public QGraphicsView {
        Q_OBJECT
    public:
        explicit MapView (QWidget *parent = nullptr);
        ~MapView () override;
        void setDataProvider (DataProvider *provider);
        void centerOwnAircraft ();
        void setAttachedChart (AttachedChart chart);
        void clearAttachedChart ();
        [[nodiscard]] bool hasAttachedChart () const noexcept;

    protected:
        void drawForeground (QPainter *painter, const QRectF &rect) override;
        void mousePressEvent (QMouseEvent *event) override;
        void mouseMoveEvent (QMouseEvent *event) override;
        void mouseReleaseEvent (QMouseEvent *event) override;
        void resizeEvent (QResizeEvent *event) override;
        void scrollContentsBy (int dx, int dy) override;

    private:
        void reloadDatabase (const QString &databasePath);
        void updateViewport (const Point2D &center, bool fitViewport, bool forceRebuild = false);
        void scheduleViewportUpdate ();
        void attachManagedItems ();
        void applyColorTheme (bool dark);
        void showMessage (const QString &message);
        void positionZoomControls ();
        void positionDetailsPanel ();
        void showDetails (const QString &text);
        void showDetailsAt (const QPoint &viewportPosition);
        void showAircraftDetails (std::size_t index);
        void setZoomLevel (int level);
        void updateZoomControls ();
        void onDataUpdated ();
        void updateAttachedChart ();
        void updateMapItemLabels ();

        [[nodiscard]] Rect2D geographicViewport (const Point2D &center) const;
        [[nodiscard]] Point2D geographicCenterFromView () const;
        [[nodiscard]] QRectF projectedViewport (const Rect2D &bound) const;

        struct AircraftHitRegion {
            std::size_t index{};
            QRectF rect;
        };

        QGraphicsScene *scene{};
        std::unique_ptr<MapItemManage> itemManager;
        QPointer<DataProvider> dataProvider;
        QPixmap ownAircraftPixmap;
        QPixmap trafficAircraftPixmap;
        QFrame *detailsPanel{};
        QTextEdit *detailsTextEdit{};
        QToolButton *detailsCloseButton{};
        std::vector<AircraftHitRegion> aircraftHitRegions;
        std::optional<std::size_t> selectedAircraft;
        std::optional<AttachedChart> attachedChart;
        QGraphicsPixmapItem *attachedChartItem{};
        Point2D geographicCenter{29.73394, 106.63437};
        QPointF projectedCenter{};
        QString loadedDatabasePath;
        bool updatingView{false};
        bool viewportUpdatePending{false};
        bool mousePanning{false};
        bool mouseDragged{false};
        bool darkTheme{false};
        bool followAircraft{true};
        QPoint lastMousePosition;
        QPoint mousePressPosition;
        QToolButton *zoomInButton{};
        QToolButton *zoomOutButton{};
        MapZoomLevel zoomLevel{MapZoomLevel::nm50};
};

#endif //CHARTNAVIGATION_ENHANCEDMAP_HPP
