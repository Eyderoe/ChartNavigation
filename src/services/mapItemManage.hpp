#ifndef CHARTNAVIGATION_MAPITEMMANAGE_HPP
#define CHARTNAVIGATION_MAPITEMMANAGE_HPP

#include <QBrush>
#include <QGraphicsItem>
#include <QGraphicsPathItem>
#include <QPainterPath>
#include <QPen>
#include <QRectF>
#include <QString>

#include <array>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include "mapDataQuery.hpp"


class DataProvider;
class QGraphicsSimpleTextItem;

QPainterPath airportSymbol();
QPainterPath fixSymbol();
QPainterPath vorSymbol();
QPainterPath dmeSymbol();
QPainterPath vordmeSymbol();
QPainterPath ndbSymbol();
QPainterPath moraSymbol();
QPainterPath firSymbol();
QPainterPath awySymbol();


enum class MapItemDetail { full, symbolOnly };

/**
 * @brief 航路、FIR 等线状地图元素。
 * @note Item 保存对应的原始数据，点击命中后不需要再次查询数据库。
 */
class MapPathItem final : public QGraphicsPathItem {
    public:
        MapPathItem (MapItemData data, const QPainterPath &path, QGraphicsItem *parent = nullptr);
        MapPathItem (std::vector<MapItemData> data, const QPainterPath &path,
                     std::vector<QPointF> labelAnchors, QGraphicsItem *parent = nullptr);

        [[nodiscard]] QRectF boundingRect () const override;
        [[nodiscard]] QPainterPath shape () const override;
        [[nodiscard]] const MapItemData& mapData () const noexcept;
        [[nodiscard]] const std::vector<MapItemData>& mapDataItems () const noexcept;
        [[nodiscard]] const MapItemData* findData (MapItemType type, int id) const noexcept;
        [[nodiscard]] MapItemType itemType () const noexcept;
        [[nodiscard]] int itemId () const noexcept;
        [[nodiscard]] QString label () const;

        void setDetail (MapItemDetail detail);
        [[nodiscard]] MapItemDetail detail () const noexcept;
        void setLabelColor (const QColor &color);
        void setPath (const QPainterPath &path);
    private:
        std::vector<MapItemData> dataItems;
        std::vector<QGraphicsSimpleTextItem*> labelItems;
        MapItemDetail currentDetail{MapItemDetail::full};
};

/**
 * @brief 航点、导航台、机场、MORA 等点状或面状地图元素。
 */
class MapPointItem final : public QGraphicsItem {
    public:
        MapPointItem (MapItemData data, QPainterPath symbol, bool screenFixed = true,
                      QGraphicsItem *parent = nullptr);

        [[nodiscard]] QRectF boundingRect () const override;
        [[nodiscard]] QPainterPath shape () const override;
        void paint (QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

        [[nodiscard]] const MapItemData& mapData () const noexcept;
        [[nodiscard]] MapItemType itemType () const noexcept;
        [[nodiscard]] int itemId () const noexcept;
        [[nodiscard]] QString label () const;

        void setSymbol (const QPainterPath &symbol);
        [[nodiscard]] const QPainterPath& symbol () const noexcept;
        void setPen (const QPen &pen);
        [[nodiscard]] QPen pen () const;
        void setBrush (const QBrush &brush);
        [[nodiscard]] QBrush brush () const;
        void setDetail (MapItemDetail detail);
        [[nodiscard]] MapItemDetail detail () const noexcept;
        void setLabelColor (const QColor &color);

    private:
        MapItemData data;
        QPainterPath symbolPath;
        QPen itemPen;
        QBrush itemBrush;
        QGraphicsSimpleTextItem *labelItem{};
        MapItemDetail currentDetail{MapItemDetail::full};
};

/**
 * @brief 静态地图图元缓存及投影管理。
 *
 * viewport 为 1 倍范围，管理器缓存 2 倍范围的 QGraphicsItem；MapDataQuery
 * 再把该范围扩张到 1.5 倍，因此数据库缓存最终覆盖约 3 倍 viewport。
 */
class MapItemManage {
    public:
        explicit MapItemManage (const QString &databaseFilePath, DataProvider *provider = nullptr);
        MapItemManage (DataProvider *provider, const QString &databaseFilePath);
        ~MapItemManage () = default;

        MapItemManage (const MapItemManage&) = delete;
        MapItemManage& operator= (const MapItemManage&) = delete;
        MapItemManage (MapItemManage&&) = delete;
        MapItemManage& operator= (MapItemManage&&) = delete;

        /**
         * @return 图元缓存发生重建时为 true；仍命中现有缓存或参数无效时为 false。
         */
        bool updateViewport (const Rect2D &viewportBound);
        bool refresh (const Rect2D &viewportBound);
        void clear () noexcept;

        [[nodiscard]] bool hasCache () const noexcept;
        [[nodiscard]] const Rect2D& itemBound () const noexcept;
        [[nodiscard]] const QRectF& projectedBound () const noexcept;
        /**
         * @note 管理器拥有返回的图元。可将裸指针加入 QGraphicsScene，但 scene 必须比管理器活得更久。
         */
        [[nodiscard]] const std::vector<std::unique_ptr<QGraphicsItem>>& items () const noexcept;
        [[nodiscard]] QGraphicsItem* findItem (MapItemType type, int id) const noexcept;
        [[nodiscard]] const MapItemData* findData (MapItemType type, int id) const noexcept;
        [[nodiscard]] const MapItemData* dataForItem (const QGraphicsItem *item) const noexcept;

        [[nodiscard]] std::vector<Point2D> project (std::vector<Point2D> positions) const;

        void setDetail (MapItemDetail detail);
        [[nodiscard]] MapItemDetail detail () const noexcept;
        void setSymbol (MapItemType type, const QPainterPath &symbol);
        void setNavaidSymbol (NavaidType type, const QPainterPath &symbol);

        void setDataProvider (DataProvider *provider) noexcept;
        [[nodiscard]] DataProvider* getDataProvider () const noexcept;

    private:
        using ItemIndexKey = std::uint64_t;

        [[nodiscard]] static ItemIndexKey indexKey (MapItemType type, int id) noexcept;
        void rebuildIndex ();
        void applyCurrentSymbol (MapPointItem &item) const;

        DataProvider *dataProvider{}; // 获取飞行器信息，不拥有该对象
        MapDataQuery query; // 获取静态地图元素
        DynamicLCC projection;
        Rect2D cachedItemBound{};
        QRectF cachedProjectedBound{};
        bool cacheValid{false};
        MapItemDetail currentDetail{MapItemDetail::full};
        std::vector<std::unique_ptr<QGraphicsItem>> cachedItems;
        std::unordered_map<ItemIndexKey, QGraphicsItem*> itemIndex;
        std::array<QPainterPath, 6> itemSymbols;
        std::array<QPainterPath, 4> navaidSymbols;
};

#endif //CHARTNAVIGATION_MAPITEMMANAGE_HPP
