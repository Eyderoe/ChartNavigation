#ifndef CHARTNAVIGATION_ENHANCEDMAP_HPP
#define CHARTNAVIGATION_ENHANCEDMAP_HPP

#include <QGraphicsView>

#include <memory>

#include "utils/geographic.hpp"


class MapItemManage;
class QGraphicsScene;
class QMouseEvent;
class QResizeEvent;

class MapView : public QGraphicsView {
        Q_OBJECT
    public:
        explicit MapView (QWidget *parent = nullptr);
        ~MapView () override;

    protected:
        void mousePressEvent (QMouseEvent *event) override;
        void mouseMoveEvent (QMouseEvent *event) override;
        void mouseReleaseEvent (QMouseEvent *event) override;
        void resizeEvent (QResizeEvent *event) override;
        void scrollContentsBy (int dx, int dy) override;

    private:
        void reloadDatabase (const QString &databasePath);
        void updateViewport (const Point2D &center, bool fitViewport);
        void scheduleViewportUpdate ();
        void attachManagedItems ();
        void applyColorTheme (bool dark);
        void showMessage (const QString &message);

        [[nodiscard]] Rect2D geographicViewport (const Point2D &center) const;
        [[nodiscard]] Point2D geographicCenterFromView () const;
        [[nodiscard]] QRectF projectedViewport (const Rect2D &bound) const;

        QGraphicsScene *scene{};
        std::unique_ptr<MapItemManage> itemManager;
        Point2D geographicCenter{29.73394, 106.63437};
        QPointF projectedCenter{};
        QString loadedDatabasePath;
        bool updatingView{false};
        bool viewportUpdatePending{false};
        bool mousePanning{false};
        bool darkTheme{false};
        QPoint lastMousePosition;
};

#endif //CHARTNAVIGATION_ENHANCEDMAP_HPP
