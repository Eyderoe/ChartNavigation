#include "enhancedMap.hpp"

#include <QFileInfo>
#include <QApplication>
#include <QFrame>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QGraphicsTextItem>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QScopedValueRollback>
#include <QScrollBar>
#include <QTimer>
#include <QToolButton>
#include <QTextEdit>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <variant>

#include "services/mapItemManage.hpp"
#include "services/dataProvider.hpp"
#include "services/settingManage.hpp"
#include "utils/constValue.hpp"


namespace {

constexpr std::array<double, 4> zoomLongEdgesNauticalMiles{25.0, 50.0, 100.0, 200.0};
constexpr qreal airportSymbolLineWidth{12.0 / 5.0};
constexpr qreal fixSymbolLineWidth{10.0 / 6.0};

constexpr int zoomLevelIndex (const MapZoomLevel level) noexcept {
    return static_cast<int>(level);
}

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
                return {QColor(QStringLiteral("#005A9C")), QColor(225, 225, 225),
                        QBrush(QColor(QStringLiteral("#005A9C"))), airportSymbolLineWidth};
            case MapItemType::awy:
                return {QColor(90, 150, 255), QColor(225, 225, 225)};
            case MapItemType::fir:
                return {QColor(QStringLiteral("#606080")), QColor(225, 225, 225),
                        Qt::NoBrush, 5.0};
            case MapItemType::fix:
                return {QColor(230, 230, 230), QColor(225, 225, 225),
                        Qt::NoBrush, fixSymbolLineWidth};
            case MapItemType::mora:
                return {QColor(115, 115, 115), QColor(225, 225, 225)};
            case MapItemType::navaid:
                return {QColor(210, 110, 230), QColor(225, 225, 225)};
        }
    }

    constexpr qreal lightLineWidth{1.25};
    switch (type) {
        case MapItemType::airport:
            return {QColor(QStringLiteral("#006B8F")), QColor(QStringLiteral("#17212B")),
                    QBrush(QColor(QStringLiteral("#006B8F"))), airportSymbolLineWidth};
        case MapItemType::awy:
            return {QColor(QStringLiteral("#245AA5")), Qt::black,
                    Qt::NoBrush, lightLineWidth};
        case MapItemType::fir:
            return {QColor(QStringLiteral("#55556F")), QColor(QStringLiteral("#303044")),
                    Qt::NoBrush, 5.0};
        case MapItemType::fix:
            return {QColor(QStringLiteral("#263238")), QColor(QStringLiteral("#17212B")),
                    Qt::NoBrush, fixSymbolLineWidth};
        case MapItemType::mora:
            return {QColor(QStringLiteral("#747B83")), QColor(QStringLiteral("#454B52"))};
        case MapItemType::navaid:
            return {QColor(QStringLiteral("#8A278F")), Qt::black,
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

bool aircraftVisible (const Point2D &position, const Point2D &ownPosition,
                      const float altitude, const float ownAltitude, const TcasMode mode) {
    const double distance = distanceSimple(position, ownPosition);
    const double altitudeDifference = std::abs(altitude - ownAltitude) * m2ft;
    switch (mode) {
        case TcasMode::none:
            return false;
        case TcasMode::nm30:
            return distance <= 30.0 * nm2m && altitudeDifference <= 9900.0;
        case TcasMode::nm6:
            return distance <= 6.0 * nm2m && altitudeDifference <= 1200.0;
        case TcasMode::all:
            return true;
    }
    return false;
}

QString coordinateText (const Point2D &position) {
    return QStringLiteral("纬度：%1<br>经度：%2")
        .arg(position.first, 0, 'f', 5)
        .arg(position.second, 0, 'f', 5);
}

QString shownText (const QString &value) {
    return value.trimmed().isEmpty() ? QStringLiteral("—") : value.trimmed();
}

QString escapedText (const QString &value) {
    return shownText(value).toHtmlEscaped();
}

QString navaidTypeText (const NavaidType type) {
    switch (type) {
        case NavaidType::vor:
            return QStringLiteral("VOR");
        case NavaidType::dme:
            return QStringLiteral("DME");
        case NavaidType::vordme:
            return QStringLiteral("VOR/DME");
        case NavaidType::ndb:
            return QStringLiteral("NDB");
    }
    return QStringLiteral("—");
}

QString detailsText (const MapItemDetails &details) {
    return std::visit([](const auto &item) {
        using T = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<T, MapFixDetails>) {
            return QStringLiteral(
                "<div class='detail-title'>航点</div>"
                "<div class='detail-body'>名称：%1<br>%2</div>")
                .arg(escapedText(item.ident), coordinateText(item.position));
        } else if constexpr (std::is_same_v<T, MapAirportDetails>) {
            return QStringLiteral(
                "<div class='detail-title'>机场</div>"
                "<div class='detail-body'>ICAO：%1<br>名称：%2<br>高度：%3英尺<br>"
                "最长跑道：%4米<br>%5</div>")
                .arg(escapedText(item.icao), escapedText(item.name))
                .arg(item.altitudeFeet)
                .arg(item.longestRunwayMetres)
                .arg(coordinateText(item.position));
        } else {
            const QString frequency = item.type == NavaidType::ndb
                                        ? QStringLiteral("%1 kHz").arg(static_cast<int>(std::lround(item.frequency)))
                                        : QStringLiteral("%1 MHz").arg(item.frequency, 0, 'f', 2);
            const QString altitude = item.type == NavaidType::ndb
                                       ? QString{}
                                       : QStringLiteral("高度：%1英尺<br>").arg(item.altitudeFeet);
            return QStringLiteral(
                "<div class='detail-title'>导航台</div>"
                "<div class='detail-body'>识别：%1<br>名称：%2<br>类型：%3<br>频率：%4<br>"
                "%5%6</div>")
                .arg(escapedText(item.ident), escapedText(item.name), navaidTypeText(item.type), frequency)
                .arg(altitude)
                .arg(coordinateText(item.position));
        }
    }, details);
}

QString relativeAltitudeText (const float altitude, const float ownAltitude, const float verticalSpeed) {
    if (!std::isfinite(altitude) || !std::isfinite(ownAltitude))
        return QStringLiteral("—");
    const double deltaAltitude = (altitude - ownAltitude) * m2ft;
    const int hundredsOfFeet = static_cast<int>(std::round(deltaAltitude / 100.0));
    QString text = QStringLiteral("%1%2")
                       .arg(hundredsOfFeet >= 0 ? QStringLiteral("+") : QStringLiteral("-"))
                       .arg(std::abs(hundredsOfFeet), 2, 10, QLatin1Char('0'));
    if (verticalSpeed >= 500.0F)
        text += QStringLiteral("↑");
    else if (verticalSpeed <= -500.0F)
        text += QStringLiteral("↓");
    return text;
}

QRectF drawAircraftLabel (QPainter &painter, const QPointF &aircraftPosition,
                          const std::vector<QString> &lines) {
    if (lines.empty())
        return {};
    QFont font = painter.font();
    font.setBold(true);
    font.setPixelSize(13);
    QPainterPath textPath;
    constexpr qreal lineHeight{14.0};
    qreal baselineOffset = 5.0 - lineHeight * (static_cast<qreal>(lines.size()) - 1.0) / 2.0;
    for (const QString &line : lines) {
        if (!line.isEmpty())
            textPath.addText(aircraftPosition + QPointF{18.0, baselineOffset}, font, line);
        baselineOffset += lineHeight;
    }
    painter.strokePath(textPath, QPen(QColor(0, 0, 0, 245), 4.0,
                                     Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.fillPath(textPath, Qt::white);
    return textPath.boundingRect();
}

void drawAircraftTrail (QPainter &painter, const std::deque<Point2D> &points,
                        const MapItemManage &itemManager, const QTransform &sceneToDevice) {
    if (points.size() < 2)
        return;

    std::vector<Point2D> geographicPoints;
    geographicPoints.reserve(points.size());
    for (const Point2D &point : points) {
        if (allFinite(point) && std::abs(point.first) <= maxSupportLat)
            geographicPoints.push_back(point);
    }
    if (geographicPoints.size() < 2)
        return;

    const auto projectedPoints = itemManager.project(std::move(geographicPoints));
    if (projectedPoints.size() < 2)
        return;

    std::vector<QPointF> devicePoints;
    devicePoints.reserve(projectedPoints.size());
    for (const auto &[x, y] : projectedPoints) {
        if (std::isfinite(x) && std::isfinite(y))
            devicePoints.push_back(sceneToDevice.map(QPointF{x, y}));
    }
    if (devicePoints.size() < 2)
        return;

    QPainterPath path(devicePoints.front());
    for (std::size_t index = 0; index + 1 < devicePoints.size(); ++index) {
        const QPointF &p0 = devicePoints[index == 0 ? 0 : index - 1];
        const QPointF &p1 = devicePoints[index];
        const QPointF &p2 = devicePoints[index + 1];
        const QPointF &p3 = devicePoints[std::min(index + 2, devicePoints.size() - 1)];
        path.cubicTo(p1 + (p2 - p0) / 6.0, p2 - (p3 - p1) / 6.0, p2);
    }

    const QColor color(239, 142, 92);
    QLinearGradient gradient(devicePoints.back(), devicePoints.front());
    gradient.setColorAt(0.0, QColor(color.red(), color.green(), color.blue(), 200));
    gradient.setColorAt(1.0, QColor(color.red(), color.green(), color.blue(), 100));
    painter.setPen(QPen(QBrush(gradient), 3.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
}

} // namespace


MapView::MapView (QWidget *parent) : QGraphicsView(parent), scene(new QGraphicsScene(this)) {
    setScene(scene);
    setRenderHint(QPainter::Antialiasing);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // 平移由下面的鼠标事件自行处理，不启用 QGraphicsView 自带的抓手光标。
    setDragMode(QGraphicsView::NoDrag);
    setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    viewport()->setCursor(Qt::ArrowCursor);
    ownAircraftPixmap.load(QStringLiteral(":/map/resources/plane_small.png"));
    trafficAircraftPixmap.load(QStringLiteral(":/map/resources/plane_small_2.png"));

    // 浮层挂在 QGraphicsView 本身，而不是它的绘图 viewport 上。viewport 会在滚动和
    // 场景重绘中刷新；把普通 QWidget 放进去会出现点击后不绘制、下一次移动才突然出现。
    detailsPanel = new QFrame(this);
    detailsPanel->setObjectName(QStringLiteral("mapDetailsPanel"));
    detailsPanel->setVisible(false);
    auto *panelLayout = new QVBoxLayout(detailsPanel);
    panelLayout->setContentsMargins(18, 18, 36, 18);
    detailsCloseButton = new QToolButton(detailsPanel);
    detailsCloseButton->setText(QStringLiteral("×"));
    detailsCloseButton->setToolTip(tr("关闭详情"));
    detailsCloseButton->setAutoRaise(true);
    detailsTextEdit = new QTextEdit(detailsPanel);
    detailsTextEdit->setReadOnly(true);
    detailsTextEdit->setFrameStyle(QFrame::NoFrame);
    detailsTextEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    detailsTextEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    detailsTextEdit->setLineWrapMode(QTextEdit::WidgetWidth);
    detailsTextEdit->document()->setDocumentMargin(0.0);
    qreal defaultPointSize = detailsTextEdit->font().pointSizeF();
    if (defaultPointSize <= 0.0)
        defaultPointSize = QApplication::font().pointSizeF();
    if (defaultPointSize <= 0.0)
        defaultPointSize = 13.0;
    detailsTextEdit->document()->setDefaultStyleSheet(QStringLiteral(
        ".detail-title { font-size: %1pt; font-weight: bold; margin-bottom: 8px; }"
        ".detail-body { font-size: %2pt; }")
        .arg(defaultPointSize * 1.5, 0, 'f', 2)
        .arg(defaultPointSize, 0, 'f', 2));
    panelLayout->addWidget(detailsTextEdit, 1);
    connect(detailsCloseButton, &QToolButton::clicked, this, [this] {
        selectedAircraft.reset();
        detailsPanel->hide();
    });

    zoomInButton = new QToolButton(this);
    zoomInButton->setText(QStringLiteral("+"));
    zoomInButton->setToolTip(tr("放大地图"));
    zoomOutButton = new QToolButton(this);
    zoomOutButton->setText(QStringLiteral("−"));
    zoomOutButton->setToolTip(tr("缩小地图"));

    connect(zoomInButton, &QToolButton::clicked, this, [this] {
        setZoomLevel(zoomLevelIndex(zoomLevel) - 1);
    });
    connect(zoomOutButton, &QToolButton::clicked, this, [this] {
        setZoomLevel(zoomLevelIndex(zoomLevel) + 1);
    });

    auto &settings = SettingsManager::instance();
    bool zoomLevelValid{};
    const int savedZoomLevel = settings.get(SettingsManager::enrouteZoomLevel,
                                            zoomLevelIndex(MapZoomLevel::nm50)).toInt(&zoomLevelValid);
    if (zoomLevelValid && savedZoomLevel >= zoomLevelIndex(MapZoomLevel::nm25)
        && savedZoomLevel <= zoomLevelIndex(MapZoomLevel::nm200))
        zoomLevel = static_cast<MapZoomLevel>(savedZoomLevel);

    const QVariant savedCenter = settings.get(SettingsManager::enrouteCenter);
    if (savedCenter.canConvert<QPointF>()) {
        const QPointF center = savedCenter.toPointF();
        if (allFinite(center.x(), center.y()) && std::abs(center.y()) <= maxSupportLat)
            geographicCenter = {center.y(), normalizeLongitude(center.x())};
    }

    updateZoomControls();
    positionZoomControls();

    connect(&settings, qOverload<SettingsManager::ConstKey, const QVariant&>(&SettingsManager::settingChanged), this,
            [this](const SettingsManager::ConstKey key, const QVariant &value) {
                switch (key) {
                    case SettingsManager::airacPath:
                        reloadDatabase(value.toString());
                        break;
                    case SettingsManager::planeFollowed:
                        followAircraft = value.toBool();
                        break;
                    default:
                        break;
                }
            });
    connect(&settings, qOverload<SettingsManager::TempKey, const QVariant&>(&SettingsManager::settingChanged), this,
            [this](const SettingsManager::TempKey key, const QVariant &value) {
                if (key == SettingsManager::isDarkTheme)
                    applyColorTheme(value.toBool());
            });

    // 等布局给出 viewport 的最终尺寸后，再按恢复的缩放等级做第一次范围查询。
    QTimer::singleShot(0, this, [this] {
        reloadDatabase(SettingsManager::instance().get(SettingsManager::airacPath, {}).toString());
    });
}

MapView::~MapView () {
    auto &settings = SettingsManager::instance();
    settings.set(SettingsManager::enrouteZoomLevel, zoomLevelIndex(zoomLevel), true);

    const Point2D center = geographicCenterFromView();
    if (allFinite(center) && std::abs(center.first) <= maxSupportLat)
        settings.set(SettingsManager::enrouteCenter,
                     QPointF{normalizeLongitude(center.second), center.first}, true);

    // scene 同时登记了 MapItemManage 持有的图元和 scene 自己持有的附件图元。
    // 先让 view 脱离 scene，再按各自所有权释放，避免退出时 scene/view 的析构回调
    // 访问另一方正在销毁的对象（Debug Qt 对这种顺序尤其敏感）。
    setScene(nullptr);
    itemManager.reset();
    attachedChartItem = nullptr;
    scene->clear();
}

void MapView::setDataProvider (DataProvider *provider) {
    if (dataProvider == provider)
        return;
    if (dataProvider)
        disconnect(dataProvider, nullptr, this, nullptr);
    dataProvider = provider;
    selectedAircraft.reset();
    if (!dataProvider && detailsPanel)
        detailsPanel->hide();
    if (dataProvider)
        connect(dataProvider, &DataProvider::dataUpdated, this, &MapView::onDataUpdated);
    viewport()->update();
}

void MapView::centerOwnAircraft () {
    if (!itemManager || !dataProvider || !dataProvider->isConnected()
        || dataProvider->getAvailableNum() <= 0)
        return;

    const Point2D position{dataProvider->getLatValues()[0], dataProvider->getLonValues()[0]};
    if (allFinite(position) && std::abs(position.first) <= maxSupportLat)
        updateViewport(position, true);
}

void MapView::setAttachedChart (AttachedChart chart) {
    attachedChart = std::move(chart);
    updateAttachedChart();
    updateMapItemLabels();
}

void MapView::clearAttachedChart () {
    attachedChart.reset();
    if (attachedChartItem) {
        scene->removeItem(attachedChartItem);
        delete attachedChartItem;
        attachedChartItem = nullptr;
    }
    updateMapItemLabels();
    viewport()->update();
}

bool MapView::hasAttachedChart () const noexcept {
    return attachedChart.has_value();
}

void MapView::drawForeground (QPainter *painter, const QRectF &rect) {
    QGraphicsView::drawForeground(painter, rect);
    aircraftHitRegions.clear();
    if (!itemManager || !dataProvider || !dataProvider->isConnected())
        return;

    const size_t available = std::min<size_t>(dataProvider->getAvailableNum(), 64);
    if (available == 0)
        return;

    const auto &latitudes = dataProvider->getLatValues();
    const auto &longitudes = dataProvider->getLonValues();
    const auto &altitudes = dataProvider->getAltValues();
    const auto &tracks = dataProvider->getTrkValues();
    const auto &flightIds = dataProvider->getFlightIdValues();
    const Point2D ownPosition{latitudes[0], longitudes[0]};
    if (!allFinite(ownPosition) || std::abs(ownPosition.first) > maxSupportLat)
        return;

    std::vector<Point2D> geographicPositions;
    geographicPositions.reserve(available);
    for (size_t index = 0; index < available; ++index)
        geographicPositions.emplace_back(latitudes[index], longitudes[index]);
    const auto projectedPositions = itemManager->project(std::move(geographicPositions));
    if (projectedPositions.size() != available)
        return;

    const QTransform sceneToDevice = painter->worldTransform();
    painter->save();
    painter->setWorldTransform(QTransform{});
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    const bool isFarthestZoom = zoomLevel == MapZoomLevel::nm200;
    if (dataProvider->getShowTrail() && !isFarthestZoom) {
        for (size_t index = 1; index < available; ++index) {
            const Point2D geographicPosition{latitudes[index], longitudes[index]};
            const Point2D projectedPosition = projectedPositions[index];
            if (!allFinite(geographicPosition, projectedPosition)
                || std::abs(geographicPosition.first) > maxSupportLat
                || !aircraftVisible(geographicPosition, ownPosition, altitudes[index], altitudes[0],
                                    dataProvider->getTcasMode()))
                continue;
            const auto flightId = slice<std::string>(flightIds, static_cast<int>(index));
            if (!flightId.empty())
                drawAircraftTrail(*painter, dataProvider->getPoints(flightId), *itemManager, sceneToDevice);
        }
    }
    for (size_t index = 0; index < available; ++index) {
        const Point2D geographicPosition{latitudes[index], longitudes[index]};
        const Point2D projectedPosition = projectedPositions[index];
        if (!allFinite(geographicPosition, projectedPosition)
            || std::abs(geographicPosition.first) > maxSupportLat)
            continue;
        if (index != 0 && !aircraftVisible(geographicPosition, ownPosition, altitudes[index], altitudes[0],
                                           dataProvider->getTcasMode()))
            continue;

        const QPixmap &pixmap = index == 0 ? ownAircraftPixmap : trafficAircraftPixmap;
        if (pixmap.isNull())
            continue;
        const QPointF devicePosition = sceneToDevice.map(
            QPointF{projectedPosition.first, projectedPosition.second});
        const qreal scale = index == 0 ? 0.4 : 0.3;
        const QSizeF aircraftSize{pixmap.width() * scale, pixmap.height() * scale};
        QRectF hitRect(devicePosition - QPointF{aircraftSize.width() / 2.0, aircraftSize.height() / 2.0},
                       aircraftSize);
        painter->save();
        painter->translate(devicePosition);
        double track = std::isfinite(tracks[index])
                         ? std::fmod(static_cast<double>(tracks[index]) + 720.0, 360.0)
                         : 0.0;
        if (dataProvider->getUseCalGeo()) {
            const int calculatedHeading = dataProvider->getGeoHeading(
                slice<std::string>(flightIds, static_cast<int>(index)));
            if (calculatedHeading != -1)
                track = calculatedHeading;
        }
        painter->rotate(track);
        painter->scale(scale, scale);
        painter->drawPixmap(-pixmap.width() / 2, -pixmap.height() / 2, pixmap);
        painter->restore();
        QRectF labelRect;
        if (index != 0 && zoomLevel == MapZoomLevel::nm50) {
            float verticalSpeed = dataProvider->getVsValues()[index];
            if (dataProvider->getUseCalVerticalSpeed()) {
                const std::string flightId = slice<std::string>(flightIds, static_cast<int>(index));
                verticalSpeed = static_cast<float>(dataProvider->getVerticalSpeed(flightId));
            }
            labelRect = drawAircraftLabel(
                *painter, devicePosition,
                {relativeAltitudeText(altitudes[index], altitudes[0], verticalSpeed)});
        } else if (index != 0 && zoomLevel == MapZoomLevel::nm25) {
            const std::string flightId = slice<std::string>(flightIds, static_cast<int>(index));
            float verticalSpeed = dataProvider->getVsValues()[index];
            if (dataProvider->getUseCalVerticalSpeed())
                verticalSpeed = static_cast<float>(dataProvider->getVerticalSpeed(flightId));

            std::vector<QString> labelLines{
                relativeAltitudeText(altitudes[index], altitudes[0], verticalSpeed)
            };
            if (dataProvider->getInfoMode() != InfoMode::base)
                labelLines.push_back(QString::fromStdString(flightId));
            if (dataProvider->getInfoMode() == InfoMode::full) {
                const std::string aircraftType = slice<std::string>(dataProvider->getFlightIcao(),
                                                                    static_cast<int>(index));
                const char wakeCategory = dataProvider->getWakeCategory(aircraftType);
                const QString wakeText = wakeCategory == ' '
                                           ? QStringLiteral("(%1)").arg(QString::fromStdString(aircraftType))
                                           : QString{QChar::fromLatin1(wakeCategory)};
                labelLines.push_back(QStringLiteral("%1 %2")
                                         .arg(dataProvider->getGroundSpeed(flightId))
                                         .arg(wakeText));
            }
            labelRect = drawAircraftLabel(*painter, devicePosition, labelLines);
        }
        if (!labelRect.isEmpty())
            hitRect = hitRect.united(labelRect);
        aircraftHitRegions.push_back({index, hitRect.adjusted(-4.0, -4.0, 4.0, 4.0)});
    }
    painter->restore();
}

void MapView::mousePressEvent (QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        mousePanning = true;
        mouseDragged = false;
        mousePressPosition = event->position().toPoint();
        lastMousePosition = event->position().toPoint();
        event->accept();
        return;
    }
    QGraphicsView::mousePressEvent(event);
}

void MapView::mouseMoveEvent (QMouseEvent *event) {
    if (mousePanning && event->buttons().testFlag(Qt::LeftButton)) {
        const QPoint position = event->position().toPoint();
        if ((position - mousePressPosition).manhattanLength() >= QApplication::startDragDistance())
            mouseDragged = true;
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
        if (!mouseDragged)
            showDetailsAt(event->position().toPoint());
        scheduleViewportUpdate();
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
    positionZoomControls();
    positionDetailsPanel();
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
    attachedChartItem = nullptr;
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

void MapView::updateViewport (const Point2D &center, const bool fitViewport, const bool forceRebuild) {
    if (!itemManager || viewport()->width() <= 1 || viewport()->height() <= 1)
        return;

    const Rect2D viewportBound = geographicViewport(center);
    QScopedValueRollback guard(updatingView, true);
    itemManager->setZoomLevel(zoomLevelIndex(zoomLevel));
    const bool rebuilt = forceRebuild ? itemManager->refresh(viewportBound)
                                      : itemManager->updateViewport(viewportBound);

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
    updateMapItemLabels();
}

void MapView::scheduleViewportUpdate () {
    if (mousePanning || updatingView || !itemManager || viewportUpdatePending)
        return;
    viewportUpdatePending = true;
    QTimer::singleShot(0, this, [this] {
        viewportUpdatePending = false;
        if (mousePanning || updatingView || !itemManager)
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
    updateAttachedChart();
    if (detailsPanel) {
        const QString background = dark ? QStringLiteral("rgba(35, 38, 44, 235)")
                                        : QStringLiteral("rgba(250, 252, 255, 238)");
        const QString foreground = dark ? QStringLiteral("#F2F4F7") : QStringLiteral("#17212B");
        const QString border = dark ? QStringLiteral("#606773") : QStringLiteral("#B8C2CC");
        detailsPanel->setStyleSheet(QStringLiteral(
            "QFrame#mapDetailsPanel { background: %1; color: %2; border: 1px solid %3; border-radius: 10px; }"
            "QTextEdit, QToolButton { color: %2; border: none; background: transparent; }"
            "QToolButton { font-size: 20px; }").arg(background, foreground, border));
    }
    if (!itemManager)
        return;

    for (const auto &item : itemManager->items()) {
        if (auto *pathItem = dynamic_cast<MapPathItem*>(item.get())) {
            const MapItemColors colors = itemColors(pathItem->itemType(), dark);
            QPen pen(colors.pen);
            pen.setCosmetic(true);
            pen.setWidthF(colors.width);
            if (pathItem->itemType() == MapItemType::fir) {
                pen.setCapStyle(Qt::FlatCap);
                pen.setDashPattern({2.0, 2.0});
            }
            pathItem->setPen(pen);
            pathItem->setBrush(colors.brush);
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

void MapView::updateAttachedChart () {
    if (!attachedChart || attachedChart->image.isNull() || !itemManager) {
        if (attachedChartItem)
            attachedChartItem->hide();
        return;
    }

    const auto &corners = attachedChart->geographicCorners;
    const auto projectedCorners = itemManager->project(
        std::vector<Point2D>(corners.begin(), corners.end()));
    if (projectedCorners.size() != corners.size()
        || !std::ranges::all_of(projectedCorners, [](const Point2D &point) { return allFinite(point); })) {
        if (attachedChartItem)
            attachedChartItem->hide();
        return;
    }

    QImage themedImage = attachedChart->image;
    if (darkTheme)
        themedImage.invertPixels(QImage::InvertRgb);
    const QPixmap pixmap = QPixmap::fromImage(std::move(themedImage));
    if (pixmap.isNull())
        return;

    const QPolygonF source{
        QPointF{0.0, 0.0}, QPointF{static_cast<qreal>(pixmap.width()), 0.0},
        QPointF{static_cast<qreal>(pixmap.width()), static_cast<qreal>(pixmap.height())},
        QPointF{0.0, static_cast<qreal>(pixmap.height())}
    };
    QPolygonF target;
    target.reserve(projectedCorners.size());
    for (const auto &[x, y] : projectedCorners)
        target.emplace_back(x, y);

    QTransform transform;
    if (!QTransform::quadToQuad(source, target, transform)) {
        if (attachedChartItem)
            attachedChartItem->hide();
        return;
    }
    if (!attachedChartItem) {
        attachedChartItem = scene->addPixmap(pixmap);
        attachedChartItem->setAcceptedMouseButtons(Qt::NoButton);
        attachedChartItem->setTransformationMode(Qt::SmoothTransformation);
        attachedChartItem->setZValue(-15.0); // 位于 FIR/MORA 之上、航路及点状元素之下。
    } else {
        attachedChartItem->setPixmap(pixmap);
    }
    attachedChartItem->setTransform(transform);
    attachedChartItem->show();
    viewport()->update();
}

void MapView::updateMapItemLabels () {
    if (!itemManager)
        return;

    QPolygonF suppressionArea;
    if (attachedChartItem && attachedChartItem->isVisible())
        suppressionArea = attachedChartItem->mapToScene(attachedChartItem->boundingRect());
    itemManager->updateDisplayPriority(viewportTransform(), viewport()->rect(), suppressionArea);
}

void MapView::showMessage (const QString &message) {
    auto *text = scene->addText(message);
    text->setDefaultTextColor(palette().color(QPalette::Text));
    const QRectF textRect = text->boundingRect().adjusted(-20.0, -20.0, 20.0, 20.0);
    scene->setSceneRect(textRect);
    centerOn(text);
}

void MapView::positionZoomControls () {
    if (!zoomInButton || !zoomOutButton)
        return;

    constexpr int rightMargin{12};
    constexpr int bottomMargin{12};
    constexpr int buttonSize{32};
    constexpr int buttonGap{4};

    const int buttonX = std::max(0, width() - rightMargin - buttonSize);
    const int zoomOutY = std::max(0, height() - bottomMargin - buttonSize);
    const int zoomInY = std::max(0, zoomOutY - buttonGap - buttonSize);
    zoomInButton->setGeometry(buttonX, zoomInY, buttonSize, buttonSize);
    zoomOutButton->setGeometry(buttonX, zoomOutY, buttonSize, buttonSize);
    zoomInButton->raise();
    zoomOutButton->raise();
}

void MapView::positionDetailsPanel () {
    if (!detailsPanel)
        return;
    constexpr int margin{12};
    const QRect viewportRect = viewport()->geometry();
    const int panelWidth = std::max(1, static_cast<int>(std::lround(viewportRect.width() * 0.30)));
    const int panelHeight = std::max(1, viewportRect.height() / 2);
    detailsPanel->setGeometry(viewportRect.left() + margin, viewportRect.top() + margin,
                              panelWidth, panelHeight);
    if (detailsCloseButton)
        detailsCloseButton->setGeometry(std::max(0, panelWidth - 34), 6, 28, 28);
    detailsPanel->raise();
    if (detailsCloseButton)
        detailsCloseButton->raise();
}

void MapView::showDetails (const QString &text) {
    if (!detailsPanel || !detailsTextEdit || text.isEmpty())
        return;
    detailsTextEdit->setHtml(text);
    detailsTextEdit->moveCursor(QTextCursor::Start);
    positionDetailsPanel();
    detailsPanel->show();
    detailsPanel->raise();
    // 详情通常在一次 mouseReleaseEvent 末尾出现，主动同步绘制，避免等到下一次
    // 鼠标移动或场景刷新时才突然显示整张卡片。
    detailsPanel->repaint();
}

void MapView::showDetailsAt (const QPoint &viewportPosition) {
    selectedAircraft.reset();
    const QPointF position(viewportPosition);
    for (auto hit = aircraftHitRegions.crbegin(); hit != aircraftHitRegions.crend(); ++hit) {
        if (hit->rect.contains(position)) {
            showAircraftDetails(hit->index);
            return;
        }
    }

    if (itemManager) {
        if (const QGraphicsItem *graphicsItem = itemAt(viewportPosition)) {
            if (const MapItemData *data = itemManager->dataForItem(graphicsItem)) {
                const MapItemType type = std::visit([](const auto &item) { return item.type; }, *data);
                const int id = std::visit([](const auto &item) { return item.id; }, *data);
                if (type == MapItemType::airport || type == MapItemType::fix || type == MapItemType::navaid) {
                    showDetails(detailsText(itemManager->itemDetails(type, id)));
                    return;
                }
            }
        }
    }
    detailsPanel->hide();
}

void MapView::showAircraftDetails (const std::size_t index) {
    if (!dataProvider || index >= 64 || index >= dataProvider->getAvailableNum()) {
        selectedAircraft.reset();
        if (detailsPanel)
            detailsPanel->hide();
        return;
    }
    selectedAircraft = index;
    const QString flightId = slice<QString>(dataProvider->getFlightIdValues(), static_cast<int>(index));
    const QString aircraftType = slice<QString>(dataProvider->getFlightIcao(), static_cast<int>(index));
    const float altitude = dataProvider->getAltValues()[index];
    const QString altitudeText = std::isfinite(altitude)
                                   ? QStringLiteral("%1英尺").arg(static_cast<int>(std::lround(altitude * m2ft)))
                                   : QStringLiteral("—");
    showDetails(QStringLiteral(
                    "<div class='detail-title'>飞机</div>"
                    "<div class='detail-body'>航班号：%1<br>机型：%2<br>高度：%3</div>")
                    .arg(escapedText(flightId), escapedText(aircraftType), altitudeText));
}

void MapView::setZoomLevel (const int level) {
    const int boundedLevel = std::clamp(level, 0, static_cast<int>(zoomLongEdgesNauticalMiles.size()) - 1);
    const auto boundedZoomLevel = static_cast<MapZoomLevel>(boundedLevel);
    const bool changed = zoomLevel != boundedZoomLevel;
    zoomLevel = boundedZoomLevel;
    updateZoomControls();
    if (changed && itemManager)
        updateViewport(geographicCenter, true, true);
}

void MapView::updateZoomControls () {
    if (!zoomInButton || !zoomOutButton)
        return;

    zoomInButton->setEnabled(zoomLevel != MapZoomLevel::nm25);
    zoomOutButton->setEnabled(zoomLevel != MapZoomLevel::nm200);
}

void MapView::onDataUpdated () {
    if (!dataProvider || !dataProvider->isConnected())
        return;
    if (mousePanning)
        return;
    if (selectedAircraft)
        showAircraftDetails(*selectedAircraft);
    if (followAircraft && itemManager && dataProvider->getAvailableNum() > 0) {
        const Point2D position{dataProvider->getLatValues()[0], dataProvider->getLonValues()[0]};
        if (allFinite(position) && std::abs(position.first) <= maxSupportLat) {
            const auto projectedPositions = itemManager->project({position});
            if (projectedPositions.size() == 1 && allFinite(projectedPositions.front())) {
                const auto &[x, y] = projectedPositions.front();
                constexpr int edge{10};
                const QRect trackingArea = viewport()->rect().adjusted(-edge, -edge, edge, edge);
                // 跟踪只维持仍在视口内的飞机；用户把飞机移出视口后，不再主动拉回。
                if (trackingArea.contains(mapFromScene(QPointF{x, y}))) {
                    updateViewport(position, true);
                    return;
                }
            }
        }
    }
    viewport()->update();
}

Rect2D MapView::geographicViewport (const Point2D &center) const {
    const double width = std::max(1, viewport()->width());
    const double height = std::max(1, viewport()->height());
    double horizontalHalfDistance{};
    double verticalHalfDistance{};
    const double longEdgeNauticalMiles = zoomLongEdgesNauticalMiles[
        static_cast<std::size_t>(zoomLevelIndex(zoomLevel))];
    if (width >= height) {
        horizontalHalfDistance = longEdgeNauticalMiles / 2.0;
        verticalHalfDistance = horizontalHalfDistance * height / width;
    } else {
        verticalHalfDistance = longEdgeNauticalMiles / 2.0;
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
    const auto geographicCenters = itemManager->unproject({
        {currentProjectedCenter.x(), currentProjectedCenter.y()}
    });
    if (geographicCenters.size() == 1 && std::isfinite(geographicCenters.front().first)
        && std::isfinite(geographicCenters.front().second))
        return geographicCenters.front();

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
