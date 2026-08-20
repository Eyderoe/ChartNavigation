#include "pdfView.hpp"
#include "utils/stringProcess.hpp"
#include "utils/constValue.hpp"
#include "utils/geographic.hpp"
#include "services/settingManage.hpp"

#include <algorithm>
#include <format>
#include <ranges>
#include <span>

#include "utils/android.hpp"


PdfView::PdfView (QWidget *parent) : QPdfView(parent) {
    initConnect();
    setPageMode(PageMode::SinglePage);
    setZoomMode(ZoomMode::Custom);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // 地图绘制
    plane.load(":/map/resources/plane_small.png");
    otherPlane.load(":/map/resources/plane_small_2.png");
}

/**
 * @brief 获取PDF文档当前页面尺寸
 * @return (长,宽) 单位:点(1/72英寸)
 */
QSizeF PdfView::getDocSize () const {
    const auto navigator = pageNavigator();
    return document()->pagePointSize(navigator->currentPage());
}

/**
 * @brief 设置是否追踪
 * @param center 居中
 */
void PdfView::setCenterOn (const bool center) {
    centerOn = center;
}

/**
 * @brief 加载仿射变换数据集
 * @param data [[lati,longi,x,y],...]
 * @param rotateDegree 机模旋转角度 (显示=实际+rotateDegree)
 * @param threshold 筛选阈值
 * @note 看 navi 才意识到, 在变换良好的情况下可以直接计算旋转角度啊, 没有写在这里的必要性
 */
void PdfView::loadMappingData (const std::vector<std::vector<double>> &data, const double rotateDegree,
                               const double threshold) {
    SettingsManager &ins = SettingsManager::instance();

    rotate = rotateDegree;
    transActive = transformer.loadData(data, threshold);
    if (!transActive) {
        ins.set(SettingsManager::affineError, NaN);
        return;
    }
    auto [error,errors] = transformer.accEvaluate();
    auto quality = transformer.squareEvaluate();
    ins.set(SettingsManager::affineError, error);
    ins.set(SettingsManager::affineQuality, static_cast<int>(quality));
    // Debug输出
    auto view = errors | std::views::transform([](double num) { return std::format("{:.2f}", num); });
    qDebug() << std::format("RMS: {:.2f}, errors: [{}]", error, join(view, ", "));
}

void PdfView::closeSimulation () const {
    if (dataProvider)
        dataProvider->closeSimu();
}

void PdfView::setDataProvider (DataProvider *provider) {
    if (dataProvider == provider)
        return;
    dataProvider = provider;
    connect(dataProvider, &DataProvider::dataUpdated, this, &PdfView::onDataUpdated);
}

void PdfView::initConnect () {
    const auto &setting = SettingsManager::instance();
    // 存储设置
    connect(&setting, qOverload<SettingsManager::ConstKey, const QVariant&>(&SettingsManager::settingChanged), this,
            [this](const SettingsManager::ConstKey key, const QVariant &val) {
                switch (key) {
                    case SettingsManager::planeFollowed: {
                        setCenterOn(val.toBool());
                        break;
                    }
                    default:
                        break;
                }
            });
    // 临时设置
    connect(&setting, qOverload<SettingsManager::TempKey, const QVariant&>(&SettingsManager::settingChanged), this,
            [this](const SettingsManager::TempKey key, const QVariant &val) {
                switch (key) {
                    case SettingsManager::isDarkTheme: {
                        setColorTheme(val.toBool());
                        break;
                    }
                    default:
                        break;
                }
            });
}

/**
 * @brief 转换经纬度至当前可视范围坐标
 * @param position <纬度,经度>
 * @return (x,y)
 */
std::pair<double, double> PdfView::trans (const Point2D &position) {
    return trans(position.first, position.second);
}

void PdfView::wheelEvent (QWheelEvent *event) {
    // 缩放计算
    const double oldZoom = zoomFactor();
    double newZoom = oldZoom;
    if (event->angleDelta().y() > 0)
        newZoom *= 1.2;
    else
        newZoom *= 0.8;
    newZoom = qBound(zoomMin, newZoom, zoomMax);
    setZoomFactor(newZoom);
    emit zoomFactor_changed(newZoom);
    // 画布缩放
    const QPointF mousePos = event->position();
    const double logicX = (horizontalScrollBar()->value() + mousePos.x()) / oldZoom;
    const double logicY = (verticalScrollBar()->value() + mousePos.y()) / oldZoom;
    const int newScrollX = static_cast<int>(logicX * newZoom - mousePos.x());
    const int newScrollY = static_cast<int>(logicY * newZoom - mousePos.y());
    horizontalScrollBar()->setValue(newScrollX);
    verticalScrollBar()->setValue(newScrollY);
    this->viewport()->update();
}

void PdfView::mousePressEvent (QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        dragging = true;
        lastPos = event->pos();
        setCursor(Qt::ClosedHandCursor);
    }
    QPdfView::mousePressEvent(event);
}

void PdfView::mouseMoveEvent (QMouseEvent *event) {
    if (dragging) {
        const QPoint delta = event->pos() - lastPos;
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        lastPos = event->pos();
    }
    QPdfView::mouseMoveEvent(event);
}

void PdfView::mouseReleaseEvent (QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        dragging = false;
        unsetCursor();
    }
    QPdfView::mouseReleaseEvent(event);
}

void PdfView::paintEvent (QPaintEvent *event) {
    QPdfView::paintEvent(event);
    QPainter painter(viewport());
    // 暗色模式逻辑
    if (isDark) {
        painter.save();
        painter.setCompositionMode(QPainter::CompositionMode_Difference);
        painter.fillRect(rect(), Qt::white);
        painter.restore();
    }

    bool check{true};
    if (!dataProvider || !dataProvider->isConnected()) // 模拟器已连接
        check = false;
    if (plane.isNull()) // 图片不可用
        check = false;
    if (!transActive) // 仿射变换可用
        check = false;
    // 飞机绘制逻辑
    if (check) {
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        for (int i = 0; i < dataProvider->getAvailableNum(); ++i)
            drawPlane(painter, i);
    }
}

/**
 * @brief 转换经纬度至当前可视范围坐标
 * @return (x,y)
 * @note 发现有一些变量可以约掉, 让ai直接重写了, 看不懂就倒回去看手写的那版
 */
std::pair<double, double> PdfView::trans (const double latitude, const double longitude) {
    auto [x, y] = transformer.transform(latitude, longitude);
    const auto viewSize = viewport()->size();
    const auto scale = zoomFactor() * screen()->logicalDotsPerInch() / 72; // PDF点 → 设备像素
    const auto logicDocSize = scale * getDocSize();
    const auto margin = documentMargins();
    const auto vertBar = verticalScrollBar(), horzBar = horizontalScrollBar();
    const auto toView = [&](const double pos, const double docSize, const QScrollBar *bar, const double margin1,
                            const double margin2, const int viewLen, const double offset) {
        if (bar->minimum() == bar->maximum())
            return offset + pos * scale;
        const double barLoc = (pos * scale + margin1) / (docSize + margin1 + margin2) * (bar->maximum() + bar->
            pageStep());
        return viewLen * (barLoc - bar->value()) / bar->pageStep();
    };
    const double finalX = toView(x, logicDocSize.width(), horzBar, margin.left(), margin.right(),
                                 viewSize.width(), (viewSize.width() - logicDocSize.width()) / 2);
    const double finalY = toView(y, logicDocSize.height(), vertBar, margin.top(), margin.bottom(),
                                 viewSize.height(), margin.top());
    return {finalX, finalY};
}


QString altInfo (const double deltaAlt, const float vs) {
    int da = static_cast<int>(std::round(deltaAlt / 100));
    QString altDescribe;
    if (deltaAlt >= 0) // 高度差
        altDescribe = QString::fromStdString(std::format("+{:02d}", da));
    else
        altDescribe = QString::fromStdString(std::format("-{:02d}", -da));
    if (vs >= 500) // 高度趋势
        altDescribe += "↑";
    else if (vs <= -500)
        altDescribe += "↓";
    else
        altDescribe += " ";
    return altDescribe;
}
bool whetherShow (const Point2D &pos1, const Point2D &pos2, const float alt1, const float alt2,
                  const TcasMode tcasMode) {
    const double _distance = distanceSimple(pos1.first, pos1.second, pos2.first, pos2.second);
    const double _alt = std::abs(alt1 - alt2) * m2ft;
    switch (tcasMode) {
        case TcasMode::none:
            return false;
        case TcasMode::nm30:
            if (_distance > nm2m * 30 || _alt > 9900)
                return false;
            return true;
        case TcasMode::nm6:
            if (_distance > nm2m * 6 || _alt > 1200)
                return false;
            return true;
        case TcasMode::all:
            return true;
        default:
            assert(false && "inop tcas mode");
    }
    return false;
}
template <typename Str>
void drawStrokedText (const int x_, const int y_, const Str &text, QPainter &painter, const QFont &font,
                      const QBrush &outlineBrush, const QBrush &textBrush) {
    QPainterPath path;
    if constexpr (std::is_same_v<Str, QString>)
        path.addText(x_, y_, font, text);
    else if constexpr (std::is_same_v<Str, std::string>)
        path.addText(x_, y_, font, QString::fromStdString(text));
    QPainterPathStroker stroker;
    stroker.setWidth(1.6);
    stroker.setCapStyle(Qt::RoundCap);
    stroker.setJoinStyle(Qt::MiterJoin);
    stroker.setMiterLimit(2.0);
    painter.setPen(Qt::NoPen);
    painter.setBrush(outlineBrush);
    const QPainterPath outline = stroker.createStroke(path).subtracted(path);
    painter.drawPath(outline);
    painter.setBrush(textBrush);
    painter.drawPath(path);
}
template void drawStrokedText<QString> (int x_, int y_, const QString &text, QPainter &painter, const QFont &font,
                                        const QBrush &outlineBrush, const QBrush &textBrush);
template void drawStrokedText<std::string> (int x_, int y_, const std::string &text, QPainter &painter,
                                            const QFont &font, const QBrush &outlineBrush, const QBrush &textBrush);
/**
 * @brief 绘制航迹: 采样25%的点(新→旧), 平滑曲线, 线宽随比例尺变化, alfa 200 → 100
 * @param painter 画笔
 * @param points 航迹点 <纬度,经度>, 尾部为最新
 * @param trans 经纬度→可视坐标转换
 * @param x 当前飞机可视坐标x
 * @param y 当前飞机可视坐标y
 * @param tooSmall 是否小比例尺
 * @param color 航迹颜色
 */
template <typename Transform>
void drawTrailLine (QPainter &painter, const std::deque<Point2D> &points, const Transform &trans,
                    const double x, const double y, const bool tooSmall, const QColor &color) {
    // 四种绘制方式：
    // 1. drawPolyline 直接连点: 折线有尖角, 无重叠圆点, 只能单色
    // 2. 分段平滑曲线: 每段独立 QPainterPath, 可逐段渐变, 但 RoundCap 重叠产生深色圆点
    // 3. 同2但改 Qt::FlatCap: 无圆点且保留渐变, 首尾平头
    // 4. 当前做法: 所有段拼成一条 QPainterPath 一次 drawPath: 无圆点, 渐变用 QLinearGradient
    const int size = static_cast<int>(points.size());
    if (size < 2)
        return;
    // 采样25%的点(新→旧)
    const int stride = 4;
    const int count = (size - 1) / stride + 1;
    std::vector<QPointF> trailPoints;
    trailPoints.reserve(count);
    for (int i = 0; i < count; ++i) {
        const auto &[lat, lon] = points[size - 1 - i * stride];
        const auto [px, py] = trans(lat, lon);
        trailPoints.emplace_back(px - x, py - y);
    }
    // 平滑曲线, 线宽随比例尺变化, alfa 200 → 100 (渐变沿轨迹方向)
    const double trailWidth = (tooSmall) ? 2.0 : 3.0;
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath path(trailPoints.front());
    for (int i = 0; i + 1 < count; ++i) {
        const auto &p0 = trailPoints[std::max(0, i - 1)];
        const auto &p1 = trailPoints[i];
        const auto &p2 = trailPoints[i + 1];
        const auto &p3 = trailPoints[std::min(i + 2, count - 1)];
        path.cubicTo(p1 + (p2 - p0) / 6, p2 - (p3 - p1) / 6, p2);
    }
    // 最新端深(200) → 最旧端浅(100)
    QLinearGradient grad(trailPoints.front(), trailPoints.back());
    grad.setColorAt(0.0, QColor(color.red(), color.green(), color.blue(), 200));
    grad.setColorAt(1.0, QColor(color.red(), color.green(), color.blue(), 100));
    painter.setPen(QPen(QBrush(grad), trailWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush); // 防止 drawPath 用残留 brush 填充路径内部
    painter.drawPath(path);
}
struct StdPlaneInfo {
    StdPlaneInfo (const DataProvider *provider, const int idx)
        : lat(provider->getLatValues()[idx]),
          lon(provider->getLonValues()[idx]),
          alt(provider->getAltValues()[idx]),
          vs(provider->getVsValues()[idx]),
          trk(provider->getTrkValues()[idx]),
          selfLat(provider->getLatValues()[0]),
          selfLon(provider->getLonValues()[0]),
          selfAlt(provider->getAltValues()[0]),
          flightId(slice<std::string>(provider->getFlightIdValues(), idx)),
          flightIcao(slice<std::string>(provider->getFlightIcao(), idx)) {}
    float lat, lon, alt, vs, trk;
    float selfLat, selfLon, selfAlt;
    std::string flightId, flightIcao;
};
/**
 * @brief 绘制自身/其他飞机
 * @param painter 画笔
 * @param idx 飞机索引(0为自身)
 * @note 后悔了, 应该改成每类信息统一绘制, 而不是每架绘制每类信息
 */
void PdfView::drawPlane (QPainter &painter, const int idx) {
    static std::map<std::string, char> turCat;
    const bool isSelf = (idx == 0);
    painter.save();
    // 飞机数据
    StdPlaneInfo info(dataProvider, idx);
    // 移动坐标系
    auto [x,y] = trans(info.lat, info.lon);
    painter.translate(x, y);
    const bool tooSmall = (zoomFactor() < 0.45);
    // 绘制信息
    if (!isSelf) {
        // 笔刷
        QFont font;
        font.setBold(true);
        painter.setFont(font);
        painter.setRenderHint(QPainter::TextAntialiasing, true);
        const QBrush outlineBrush(Qt::black);
        const QBrush textBrush(Qt::white);
        // tcas 范围判断是否显示
        const bool show = whetherShow({info.lat, info.lon}, {info.selfLat, info.selfLon},
                                      info.alt, info.selfAlt, dataProvider->getTcasMode());
        if (!show) {
            painter.restore();
            return;
        }
        // 高度信息
        const QString altDescribe = altInfo((info.alt - dataProvider->getAltValues()[0]) * m2ft, info.vs);
        // 尾流信息
        char cat;
        if (const auto itLocal = turCat.find(info.flightIcao); itLocal == turCat.end()) {
            cat = dataProvider->getWakeCategory(info.flightIcao);
            turCat[info.flightIcao] = cat;
        } else
            cat = itLocal->second;
        std::string catStr = (cat == ' ') ? std::format("({})", info.flightIcao) : std::string({cat}); // 查询不到回退到机型
        // 绘制TCAS信息
        if (!tooSmall) {
            switch (dataProvider->getInfoMode()) {
                case InfoMode::full: { // 完整符号(相对高度趋势, 航班号, 地速尾流)
                    auto gs = dataProvider->getGroundSpeed(info.flightId);
                    drawStrokedText(12, 20, std::format("{} {}", gs, catStr), painter, font, outlineBrush, textBrush);
                }
                case InfoMode::extend: // 扩展符号(相对高度趋势, 航班号)
                    drawStrokedText(12, 8, info.flightId, painter, font, outlineBrush, textBrush);
                case InfoMode::base: // 基本符号(相对高度趋势)
                    drawStrokedText(12, -4, altDescribe, painter, font, outlineBrush, textBrush);
            }
        }
        // 计算航向
        if (dataProvider->getUseCalGeo()) {
            const auto temp = static_cast<float>(dataProvider->getGeoHeading(info.flightId));
            info.trk = (temp != -1) ? temp : info.trk;
        }
        // 绘制航迹
        if (dataProvider->getShowTrail()) {
            const auto &points = dataProvider->getPoints(info.flightId);
            drawTrailLine(painter, points, [this](const double lat, const double lon) { return trans(lat, lon); },
                          x, y, tooSmall, QColor(239, 142, 92));
        }
    }
    // 安卓&现实GPS特化:
    if constexpr (platform == MultiPlatform::androidOS) {
        static AircraftTrail trail(100); // 通过邪修(因为安卓现在默认1Hz), 延长至10分钟的点数据
        bool calGeo = SettingsManager::instance().get(SettingsManager::useCalGeoHeading, false).toBool();
        bool showTrail = SettingsManager::instance().get(SettingsManager::showTrail, false).toBool();
        bool real = dataProvider->getSimulatorSource() == SimulatorSource::real;
        trail.addPoint({info.lat, info.lon});
        // 计算航向应用于自身
        if (isSelf && calGeo && real) {
            auto trkTemp = trail.calculateGeoHeading();
            if (trkTemp != -1)
                info.trk = static_cast<float>(trkTemp);
        }
        // 显示十分钟的航迹
        if (isSelf && showTrail && real) {
            const auto &points = trail.getPoints();
            drawTrailLine(painter, points, [this](const double lat, const double lon) { return trans(lat, lon); },
                          x, y, tooSmall, QColor(245, 189, 70));
        }
    }
    // 绘制飞机
    info.trk = static_cast<float>(std::fmod(info.trk + rotate + 720, 360)); // 加上航图自身旋转
    painter.rotate(info.trk);
    if (isSelf) {
        painter.scale(0.4, 0.4);
        painter.drawPixmap(-plane.width() / 2, -plane.height() / 2, plane);
    } else {
        painter.scale(0.3, 0.3);
        painter.drawPixmap(-otherPlane.width() / 2, -otherPlane.height() / 2, otherPlane);
    }
    painter.restore();
}

/**
 * @brief 设置色彩主题
 * @param darkTheme 是否使用暗色主题
 */
void PdfView::setColorTheme (const bool darkTheme) {
    isDark = darkTheme;
}

/**
 * @brief 模拟器数据更新时刷新显示
 */
void PdfView::onDataUpdated () {
    // 未连接到模拟器
    if (!dataProvider || !dataProvider->isConnected())
        return;
    // 映射不可用
    if (!transActive)
        return;
    // 不使用居中
    viewport()->update(); // 保证至少更新下, 不然自身不处于viewport()中其它不会更新
    if (!centerOn || dragging)
        return;

    // 自身居中逻辑
    auto [x,y] = trans(dataProvider->getLatValues()[0], dataProvider->getLonValues()[0]);
    constexpr double edge{10};
    if ((x < -edge) || (x > viewport()->width() + edge))
        return;
    if ((y < -edge) || (y > viewport()->height() + edge))
        return;
    const auto vertBar = verticalScrollBar(), horzBar = horizontalScrollBar();
    // 水平
    if (horzBar->minimum() != horzBar->maximum()) {
        const int deltaX = static_cast<int>(x) - viewport()->width() / 2;
        const int newPos = horzBar->value() + deltaX;
        horzBar->setValue(qBound(horzBar->minimum(), newPos, horzBar->maximum()));
    }
    // 垂直
    if (vertBar->minimum() != vertBar->maximum()) {
        const int deltaY = static_cast<int>(y) - viewport()->height() / 2;
        const int newPos = vertBar->value() + deltaY;
        vertBar->setValue(qBound(vertBar->minimum(), newPos, vertBar->maximum()));
    }
    viewport()->update();
}
