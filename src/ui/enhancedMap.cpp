#include "enhancedMap.hpp"

#include <QFileInfo>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QScopedValueRollback>
#include <QScrollBar>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <numbers>

#include "services/mapItemManage.hpp"
#include "services/settingManage.hpp"
#include "utils/constValue.hpp"


namespace {

constexpr double defaultLongEdgeNauticalMiles{50.0};

struct MapItemColors {
    QColor pen;
    QColor label;
    QBrush brush{Qt::NoBrush};
    qreal width{1.0};
};

MapItemColors itemColors (const MapItemType type, const bool dark) {
    if (dark) {
        switch (type) {
            case MapItemType::airport:
                return {QColor(80, 200, 255), QColor(225, 225, 225)};
            case MapItemType::awy:
                return {QColor(90, 150, 255), QColor(225, 225, 225)};
            case MapItemType::fir:
                return {QColor(QStringLiteral("#606080")), QColor(225, 225, 225), Qt::NoBrush, 1.5};
            case MapItemType::fix:
                return {QColor(230, 230, 230), QColor(225, 225, 225)};
            case MapItemType::mora:
                return {QColor(115, 115, 115), QColor(225, 225, 225), QBrush(QColor(90, 90, 90, 20))};
            case MapItemType::navaid:
                return {QColor(210, 110, 230), QColor(225, 225, 225)};
        }
    }

    constexpr qreal lightLineWidth{1.25};
    switch (type) {
        case MapItemType::airport:
            return {QColor(QStringLiteral("#006B8F")), QColor(QStringLiteral("#17212B")),
                    Qt::NoBrush, lightLineWidth};
        case MapItemType::awy:
            return {QColor(QStringLiteral("#245AA5")), QColor(QStringLiteral("#1D3F73")),
                    Qt::NoBrush, lightLineWidth};
        case MapItemType::fir:
            return {QColor(QStringLiteral("#55556F")), QColor(QStringLiteral("#303044")),
                    Qt::NoBrush, 1.6};
        case MapItemType::fix:
            return {QColor(QStringLiteral("#263238")), QColor(QStringLiteral("#17212B")),
                    Qt::NoBrush, lightLineWidth};
        case MapItemType::mora:
            return {QColor(QStringLiteral("#747B83")), QColor(QStringLiteral("#454B52")),
                    QBrush(QColor(65, 85, 105, 12))};
        case MapItemType::navaid:
            return {QColor(QStringLiteral("#8A278F")), QColor(QStringLiteral("#671B6B")),
                    Qt::NoBrush, lightLineWidth};
    }
    return {};
}

QRectF boundingRect (const std::vector<Point2D> &points) {
    QRectF result;
    bool initialized{};
    for (const auto &[x, y] : points) {
        if (!std::isfinite(x) || !std::isfinite(y))
            continue;
        const QRectF pointRect(QPointF{x, y}, QSizeF{});
        if (!initialized) {
            result = pointRect;
            initialized = true;
        } else {
            result |= pointRect;
        }
    }
    return result.normalized();
}

} // namespace


MapView::MapView (QWidget *parent) : QGraphicsView(parent), scene(new QGraphicsScene(this)) {
    setScene(scene);
    setRenderHint(QPainter::Antialiasing);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    viewport()->setCursor(Qt::OpenHandCursor);

    auto &settings = SettingsManager::instance();
    connect(&settings, qOverload<SettingsManager::ConstKey, const QVariant&>(&SettingsManager::settingChanged), this,
            [this](const SettingsManager::ConstKey key, const QVariant &value) {
                if (key == SettingsManager::airacPath)
                    reloadDatabase(value.toString());
            });
    connect(&settings, qOverload<SettingsManager::TempKey, const QVariant&>(&SettingsManager::settingChanged), this,
            [this](const SettingsManager::TempKey key, const QVariant &value) {
                if (key == SettingsManager::isDarkTheme)
                    applyColorTheme(value.toBool());
            });

    // 等布局给出 viewport 的最终尺寸后再做第一次 50 海里范围查询。
    QTimer::singleShot(0, this, [this] {
        reloadDatabase(SettingsManager::instance().get(SettingsManager::airacPath, {}).toString());
    });
}

MapView::~MapView () = default;

void MapView::mousePressEvent (QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        mousePanning = true;
        lastMousePosition = event->position().toPoint();
        viewport()->setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QGraphicsView::mousePressEvent(event);
}

void MapView::mouseMoveEvent (QMouseEvent *event) {
    if (mousePanning && event->buttons().testFlag(Qt::LeftButton)) {
        const QPoint position = event->position().toPoint();
        const QPoint delta = position - lastMousePosition;
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        lastMousePosition = position;
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void MapView::mouseReleaseEvent (QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && mousePanning) {
        mousePanning = false;
        viewport()->setCursor(Qt::OpenHandCursor);
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

void MapView::resizeEvent (QResizeEvent *event) {
    // resizeEvent 到达时 viewport 已经是新尺寸，而 transform 仍对应旧尺寸；此时从 view
    // 反算中心会产生巨大的假位移。缩放只改变可见范围，必须沿用已保存的地理中心。
    QScopedValueRollback guard(updatingView, true);
    QGraphicsView::resizeEvent(event);
    if (itemManager)
        updateViewport(geographicCenter, true);
}

void MapView::scrollContentsBy (const int dx, const int dy) {
    QGraphicsView::scrollContentsBy(dx, dy);
    scheduleViewportUpdate();
}

void MapView::reloadDatabase (const QString &databasePath) {
    const QString path = databasePath.trimmed();
    if (itemManager && path == loadedDatabasePath)
        return;

    QScopedValueRollback guard(updatingView, true);
    // QGraphicsItem 析构时会先从 scene 注销；因此必须先释放管理器，再清理提示图元。
    itemManager.reset();
    scene->clear();
    loadedDatabasePath.clear();

    if (path.isEmpty()) {
        showMessage(tr("请先在设置中指定 AIRAC 导航数据库。"));
        return;
    }
    if (!QFileInfo(path).isFile()) {
        showMessage(tr("AIRAC 导航数据库不存在：\n%1").arg(path));
        return;
    }

    try {
        itemManager = std::make_unique<MapItemManage>(path);
        loadedDatabasePath = path;
        updateViewport(geographicCenter, true);
    } catch (const std::exception &error) {
        itemManager.reset();
        scene->clear();
        showMessage(tr("AIRAC 导航数据库加载失败：\n%1").arg(QString::fromLocal8Bit(error.what())));
    }
}

void MapView::updateViewport (const Point2D &center, const bool fitViewport) {
    if (!itemManager || viewport()->width() <= 1 || viewport()->height() <= 1)
        return;

    const Rect2D viewportBound = geographicViewport(center);
    QScopedValueRollback guard(updatingView, true);
    const bool rebuilt = itemManager->updateViewport(viewportBound);

    if (rebuilt) {
        attachManagedItems();
        applyColorTheme(darkTheme);
        const QRectF cachedRect = itemManager->projectedBound();
        // 给 scrollbar 留出图元缓存之外的移动空间。越过缓存边缘后，下一次更新会重建缓存。
        scene->setSceneRect(cachedRect.adjusted(-cachedRect.width(), -cachedRect.height(),
                                                cachedRect.width(), cachedRect.height()));
    }

    if (rebuilt || fitViewport) {
        const auto centers = itemManager->project({center});
        const bool centerValid = centers.size() == 1 && std::isfinite(centers.front().first)
                                 && std::isfinite(centers.front().second);
        if (centerValid) {
            // fitInView() 之前先提交锚点，避免 scrollbar 引发的同步 resize 使用旧锚点。
            geographicCenter = center;
            projectedCenter = QPointF{centers.front().first, centers.front().second};
        }

        const QRectF visibleRect = projectedViewport(viewportBound);
        if (!visibleRect.isEmpty())
            fitInView(visibleRect, Qt::KeepAspectRatio);

        if (centerValid)
            centerOn(projectedCenter);
    } else {
        projectedCenter = mapToScene(viewport()->rect().center());
    }
    geographicCenter = center;
}

void MapView::scheduleViewportUpdate () {
    if (updatingView || !itemManager || viewportUpdatePending)
        return;
    viewportUpdatePending = true;
    QTimer::singleShot(0, this, [this] {
        viewportUpdatePending = false;
        if (updatingView || !itemManager)
            return;
        try {
            updateViewport(geographicCenterFromView(), false);
        } catch (const std::exception &error) {
            qWarning() << "Map viewport update failed:" << error.what();
        }
    });
}

void MapView::attachManagedItems () {
    for (const auto &item : itemManager->items()) {
        if (!item->scene())
            scene->addItem(item.get());
    }
}

void MapView::applyColorTheme (const bool dark) {
    darkTheme = dark;
    if (!itemManager)
        return;

    for (const auto &item : itemManager->items()) {
        if (auto *pathItem = dynamic_cast<MapPathItem*>(item.get())) {
            const MapItemColors colors = itemColors(pathItem->itemType(), dark);
            QPen pen(colors.pen);
            pen.setCosmetic(true);
            pen.setWidthF(colors.width);
            pathItem->setPen(pen);
            pathItem->setLabelColor(colors.label);
        } else if (auto *pointItem = dynamic_cast<MapPointItem*>(item.get())) {
            const MapItemColors colors = itemColors(pointItem->itemType(), dark);
            QPen pen(colors.pen);
            pen.setCosmetic(true);
            pen.setWidthF(colors.width);
            pointItem->setPen(pen);
            pointItem->setBrush(colors.brush);
            pointItem->setLabelColor(colors.label);
        }
    }
    viewport()->update();
}

void MapView::showMessage (const QString &message) {
    auto *text = scene->addText(message);
    text->setDefaultTextColor(palette().color(QPalette::Text));
    const QRectF textRect = text->boundingRect().adjusted(-20.0, -20.0, 20.0, 20.0);
    scene->setSceneRect(textRect);
    centerOn(text);
}

Rect2D MapView::geographicViewport (const Point2D &center) const {
    const double width = std::max(1, viewport()->width());
    const double height = std::max(1, viewport()->height());
    double horizontalHalfDistance{};
    double verticalHalfDistance{};
    if (width >= height) {
        horizontalHalfDistance = defaultLongEdgeNauticalMiles / 2.0;
        verticalHalfDistance = horizontalHalfDistance * height / width;
    } else {
        verticalHalfDistance = defaultLongEdgeNauticalMiles / 2.0;
        horizontalHalfDistance = verticalHalfDistance * width / height;
    }

    const Point2D north = pointBearingDistance(center, 0.0, verticalHalfDistance);
    const Point2D east = pointBearingDistance(center, 90.0, horizontalHalfDistance);
    const Point2D south = pointBearingDistance(center, 180.0, verticalHalfDistance);
    const Point2D west = pointBearingDistance(center, 270.0, horizontalHalfDistance);
    return {{north.first, west.second}, {south.first, east.second}};
}

Point2D MapView::geographicCenterFromView () const {
    if (!itemManager)
        return geographicCenter;
    const QPointF currentProjectedCenter = mapToScene(viewport()->rect().center());
    const double east = currentProjectedCenter.x() - projectedCenter.x();
    const double north = projectedCenter.y() - currentProjectedCenter.y();
    const double distanceNauticalMiles = std::hypot(east, north) / nm2m;
    if (!std::isfinite(distanceNauticalMiles) || distanceNauticalMiles == 0.0)
        return geographicCenter;

    double bearing = std::atan2(east, north) * 180.0 / std::numbers::pi;
    if (bearing < 0.0)
        bearing += 360.0;
    return pointBearingDistance(geographicCenter, bearing, distanceNauticalMiles);
}

QRectF MapView::projectedViewport (const Rect2D &bound) const {
    const auto &[topLeft, bottomRight] = bound;
    return boundingRect(itemManager->project({
        topLeft,
        {topLeft.first, bottomRight.second},
        bottomRight,
        {bottomRight.first, topLeft.second}
    }));
}
