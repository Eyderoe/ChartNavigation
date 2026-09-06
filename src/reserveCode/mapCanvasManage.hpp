#ifndef CHARTNAVIGATION_MAPCANVASMANAGE_HPP
#define CHARTNAVIGATION_MAPCANVASMANAGE_HPP

#include <QColor>
#include <QPainterPath>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QTransform>

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "services/mapDataQuery.hpp"


class QPainter;

namespace reserveCode {

/**
 * @brief 立即模式地图绘制器，保留给不使用 QGraphicsItem 的地图实现。
 *
 * updateViewport() 只在视口离开缓存范围时查询数据库并重建投影几何；paint()
 * 则在每一帧把缓存内容直接绘制到目标画布。绘制器同时维护数据索引、标签避让
 * 和最近一帧的命中区域，不依赖 QGraphicsScene 的图元、索引及所有权模型。
 *
 * @note 所有接口应从同一个 GUI 线程调用。findData()/dataAt() 返回的指针会在
 *       下一次缓存重建或 clear() 后失效。
 */
class MapCanvasManage final {
    public:
        explicit MapCanvasManage (const QString &databaseFilePath);

        MapCanvasManage (const MapCanvasManage&) = delete;
        MapCanvasManage& operator= (const MapCanvasManage&) = delete;
        MapCanvasManage (MapCanvasManage&&) = delete;
        MapCanvasManage& operator= (MapCanvasManage&&) = delete;

        /**
         * @return 投影几何缓存发生重建时为 true。
         */
        bool updateViewport (const Rect2D &viewportBound);
        bool refresh (const Rect2D &viewportBound);
        void clear () noexcept;

        /**
         * @brief 更新缓存并立即绘制一帧。
         * @return 本帧绘制前是否重建了投影几何缓存。
         */
        bool render (QPainter &painter, const QRectF &canvasRect, const Rect2D &viewportBound);

        /**
         * @brief 使用现有缓存绘制一帧。
         * @note 若视口不在缓存范围内或参数无效，本次不会绘制。
         */
        void paint (QPainter &painter, const QRectF &canvasRect, const Rect2D &viewportBound);

        void setZoomLevel (int level) noexcept;
        [[nodiscard]] int zoomLevel () const noexcept;
        void setLabelsVisible (bool visible) noexcept;
        [[nodiscard]] bool labelsVisible () const noexcept;
        void setDarkTheme (bool dark) noexcept;
        [[nodiscard]] bool darkTheme () const noexcept;

        [[nodiscard]] bool hasCache () const noexcept;
        [[nodiscard]] const Rect2D& itemBound () const noexcept;
        [[nodiscard]] std::size_t itemCount () const noexcept;
        [[nodiscard]] const MapItemData* findData (MapItemType type, int id) const noexcept;

        /**
         * @brief 查询最近一次 paint()/render() 中画布坐标处的元素。
         * @param canvasPosition 画布局部坐标。
         * @param tolerance 额外命中半径，单位为画布像素。
         */
        [[nodiscard]] const MapItemData* dataAt (const QPointF &canvasPosition,
                                                 qreal tolerance = 3.0) const;

        [[nodiscard]] std::vector<Point2D> project (std::vector<Point2D> positions) const;
        [[nodiscard]] std::vector<Point2D> unproject (std::vector<Point2D> positions) const;

    private:
        struct ProjectedItem {
            MapItemData data;
            std::vector<QPointF> geometry;
        };

        struct HitRegion {
            std::size_t itemIndex{};
            QPainterPath path;
        };

        using ItemIndexKey = std::uint64_t;

        [[nodiscard]] static ItemIndexKey indexKey (MapItemType type, int id) noexcept;
        [[nodiscard]] QTransform canvasTransform (const QRectF &canvasRect,
                                                  const Rect2D &viewportBound) const;
        void rebuildIndex ();

        MapDataQuery query;
        DynamicLCC projection;
        Rect2D cachedItemBound{};
        bool cacheValid{false};
        int currentZoomLevel{1};
        bool showLabels{true};
        bool useDarkTheme{false};
        std::vector<ProjectedItem> projectedItems;
        std::unordered_map<ItemIndexKey, std::size_t> itemIndex;
        std::vector<HitRegion> lastHitRegions;
};

} // namespace reserveCode

#endif //CHARTNAVIGATION_MAPCANVASMANAGE_HPP
