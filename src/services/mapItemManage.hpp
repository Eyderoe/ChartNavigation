/**
 * @file mapItemManage.hpp
 * @brief 管理地图元素本身
 */


#ifndef CHARTNAVIGATION_MAPITEMMANAGE_HPP
#define CHARTNAVIGATION_MAPITEMMANAGE_HPP

#include <QBrush>
#include <QGraphicsItem>
#include <QGraphicsPathItem>
#include <QLineF>
#include <QPainterPath>
#include <QPen>
#include <QPolygonF>
#include <QRectF>
#include <QString>

#include <array>
#include <memory>
#include <vector>

#include "mapDataQuery.hpp"


class QGraphicsSimpleTextItem;
class QTransform;

/**
 * @brief 航路、FIR 等线状地图元素。
 */
class MapPathItem final : public QGraphicsPathItem {
    public:
        MapPathItem (std::vector<MapItemData> data, const QPainterPath &path,
                     std::vector<QPointF> labelAnchors, QGraphicsItem *parent = nullptr);

        [[nodiscard]] QRectF boundingRect () const override;
        [[nodiscard]] QPainterPath shape () const override;
        void paint (QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;
        [[nodiscard]] const MapItemData& mapData () const noexcept;
        [[nodiscard]] const MapItemData* findData (MapItemType type, int id) const noexcept;
        [[nodiscard]] MapItemType itemType () const noexcept;
        [[nodiscard]] const std::vector<QGraphicsSimpleTextItem*>& labels () const noexcept;
        [[nodiscard]] std::vector<QRectF> airwayArrowBounds (const QTransform &sceneToDevice) const;

        void setLabelsVisible (bool visible);
        void setLabelColor (const QColor &color);
        void setAirwayLabelSegments (const std::vector<QLineF> &segments);
        void setAirwayLabelPosition (size_t index, qreal position);
    private:
        std::vector<MapItemData> dataItems;
        std::vector<QLineF> airwaySegments;
        std::vector<QGraphicsSimpleTextItem*> labelItems;
};

/**
 * @brief 航点、导航台、机场、MORA 等点状或面状地图元素。
 */
class MapPointItem final : public QGraphicsItem {
    public:
        MapPointItem (MapItemData data, QPainterPath symbol, bool screenFixed = true, QGraphicsItem *parent = nullptr);

        [[nodiscard]] QRectF boundingRect () const override;
        [[nodiscard]] QPainterPath shape () const override;
        void paint (QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

        [[nodiscard]] const MapItemData& mapData () const noexcept;
        [[nodiscard]] MapItemType itemType () const noexcept;
        [[nodiscard]] QString label () const;
        [[nodiscard]] QGraphicsSimpleTextItem* labelGraphicsItem () const noexcept;

        void setPen (const QPen &pen);
        void setBrush (const QBrush &brush);
        void setLabelsVisible (bool visible);
        void setLabelColor (const QColor &color);
        void setLabelAnchor (const QPointF &anchor, bool centered = false) const;
    private:
        MapItemData data;
        QPainterPath symbolPath;
        QPen itemPen;
        QBrush itemBrush;
        QGraphicsSimpleTextItem *labelItem{};
};


/**
 * @brief 静态地图图元缓存及投影管理。
 */
class MapItemManage {
    public:
        explicit MapItemManage (const QString &databaseFilePath);
        ~MapItemManage () = default;

        bool updateViewport (const Rect2D &viewportBound);
        bool refresh (const Rect2D &viewportBound);
        void setZoomLevel (int level);
        void updateDisplayPriority (const QTransform &sceneToDevice, const QRectF &deviceViewport,
                                    const QPolygonF &labelSuppressionArea = {});

        [[nodiscard]] const QRectF& projectedBound () const noexcept;
        [[nodiscard]] const std::vector<std::unique_ptr<QGraphicsItem>>& items () const noexcept;
        [[nodiscard]] const MapItemData* dataForItem (const QGraphicsItem *item) const noexcept;
        MapItemDetails itemDetails (MapItemType type, int id);

        [[nodiscard]] std::vector<Point2D> project (std::vector<Point2D> positions) const;
        [[nodiscard]] std::vector<Point2D> unproject (std::vector<Point2D> positions) const;

    private:
        [[nodiscard]] QPainterPath symbolForData (const MapItemData &data) const;
        void applyZoomPolicy ();

        MapDataQuery query;
        DynamicLCC projection;
        Rect2D cachedItemBound{};
        QRectF cachedProjectedBound{};
        bool cacheValid{false};
        bool projectionResetPending{true};
        int currentZoomLevel{1};
        std::vector<std::unique_ptr<QGraphicsItem>> cachedItems;
        QPainterPath airportSymbolPath;
        QPainterPath fixSymbolPath;
        std::array<QPainterPath, 4> navaidSymbols;
};

#endif //CHARTNAVIGATION_MAPITEMMANAGE_HPP
