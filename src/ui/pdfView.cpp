#include "pdfView.hpp"
#include "utils/stringProcess.hpp"
#include "utils/constValue.hpp"
#include "utils/geographic.hpp"
#include "services/settingManage.hpp"

#include <ranges>


PdfView::PdfView (QWidget *parent) : QPdfView(parent) {
    initConnect();
    setPageMode(PageMode::SinglePage);
    setZoomMode(ZoomMode::Custom);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // 地图绘制
    plane.load(":/map/resources/plane_small.png");
    otherPlane.load(":/map/resources/plane_small_2.png");
    // 接收器切换
    SettingsManager &ins = SettingsManager::instance();
    const SimulatorSource source = static_cast<SimulatorSource>(ins.get(SettingsManager::dataSource, 0).toInt());
    qDebug() << "data source: " << static_cast<int>(source);
    switch (source) {
        case SimulatorSource::xplane:
            connector = std::make_unique<xpAdapter>();
            break;
        case SimulatorSource::wlan:
            connector = std::make_unique<wlanAdapter>();
            break;
        case SimulatorSource::real:
            connector = std::make_unique<realAdapter>();
            break;
        default:
            throw std::invalid_argument("inop adapter");
    }
    // 模拟器
    simuInit();
    // 定时器
    simuUpdateTimer.setInterval(1000);
    connect(&simuUpdateTimer, &QTimer::timeout, this, &PdfView::simuInfoUpdate);
    simuUpdateTimer.start();
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

void PdfView::closeSimu () const {
    connector->close();
}

void PdfView::initConnect () {
    const auto &setting = SettingsManager::instance();
    // 存储设置
    connect(&setting, qOverload<SettingsManager::ConstKey, const QVariant&>(&SettingsManager::settingChanged), this,
            [this](const SettingsManager::ConstKey key, const QVariant &val) {
                switch (key) {
                    case SettingsManager::dataSource: {
                        setConnector(val.toInt());
                        break;
                    }
                    case SettingsManager::planeFollowed: {
                        setCenterOn(val.toBool());
                        break;
                    }
                    case SettingsManager::tcasRange: {
                        switch (const auto mode = static_cast<TcasMode>(val.toInt()); mode) {
                            case TcasMode::nm30:
                            case TcasMode::nm6:
                            case TcasMode::none:
                            case TcasMode::all:
                                tcasMode = mode;
                                break;
                            default:
                                tcasMode = TcasMode::nm30;
                        }
                        break;
                    }
                    case SettingsManager::infoMode: {
                        switch (const auto mode = static_cast<InfoMode>(val.toInt()); mode) {
                            case InfoMode::base:
                            case InfoMode::extend:
                            case InfoMode::full:
                                infoMode = mode;
                                break;
                            default:
                                infoMode = InfoMode::base;
                        }
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
    if (!connected) // xp已连接
        check = false;
    if (plane.isNull()) // 图片不可用
        check = false;
    if (!transActive) // 仿射变换可用
        check = false;
    // 飞机绘制逻辑
    if (check) {
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        // 自身
        drawPlane(painter);
        // 其他飞机
        const size_t count = std::ranges::count_if(multiIdVal, [](const float value) { return value != 0.0f; });
        for (int i = 1; i < count; ++i)
            drawPlane(painter, i);
    }
}

/**
 * @brief 转换经纬度至当前可视范围坐标
 * @return (x,y)
 */
std::pair<double, double> PdfView::trans (const double latitude, const double longitude) {
    // 获取文档位置
    auto [x, y] = transformer.transform(latitude, longitude);
    // 获取基本信息
    const auto viewSize = viewport()->size();
    const auto *scree = screen();
    const auto vertBar = verticalScrollBar(), horzBar = horizontalScrollBar();
    const auto dpi = scree->logicalDotsPerInch();
    const auto logicDocSize = zoomFactor() * getDocSize() * dpi / 72;
    const auto margin = documentMargins();
    // x计算
    double finalX{};
    const double scaleX = x / getDocSize().width(); // 1 PDF (点)
    double docX = scaleX * logicDocSize.width(); // 2 PDF (像素)
    if (horzBar->minimum() == horzBar->maximum()) { // 非缩放状态
        finalX = (viewSize.width() - logicDocSize.width()) / 2 + docX;
    } else { // 缩放状态
        docX += margin.left(); // 3 PDF+边缘 (像素)
        const double barLength = horzBar->maximum() + horzBar->pageStep();
        const double barLoc = docX / (logicDocSize.width() + margin.left() + margin.right()) * barLength; // 4 滚动条 (像素)
        const double windowScale = (barLoc - horzBar->value()) / horzBar->pageStep(); // 5 滑动块 (比例)
        finalX = viewSize.width() * windowScale;
    }
    // y计算
    double finalY{};
    const double scaleY = y / getDocSize().width();
    double docY = scaleY * logicDocSize.width();
    if (vertBar->minimum() == vertBar->maximum()) { // 非缩放状态
        finalY = margin.top() + docY;
    } else { // 缩放状态
        docY += margin.top(); // 3 PDF+边缘 (像素)
        const double barLength = vertBar->maximum() + vertBar->pageStep();
        const double barLoc = docY / (logicDocSize.height() + margin.top() + margin.bottom()) * barLength; // 4 滚动条 (像素)
        const double windowScale = (barLoc - vertBar->value()) / vertBar->pageStep(); // 5 滑动块 (比例)
        finalY = viewSize.height() * windowScale;
    }
    return {finalX, finalY};
}

/**
 * @brief 绘制自身/其他飞机
 * @param painter 画笔
 * @param idx 飞机索引(0为自身)
 */
void PdfView::drawPlane (QPainter &painter, const int idx) {
    const bool isSelf = (idx == 0);
    painter.save();
    // 变量声明
    const double latitude{multiLatVal[idx]}, longitude{multiLonVal[idx]}, vs{multiVsVal[idx]}, alt{multiAltVal[idx]};
    double trk{multiTrkVal[idx]};
    // 移动坐标系
    auto [x,y] = trans(latitude, longitude);
    painter.translate(x, y);
    trk = std::fmod(trk + rotate + 360, 360);
    // 绘制信息
    if (!isSelf) {
        QFont font;
        font.setBold(true);
        painter.setFont(font);
        const QPen outlinePen(Qt::black, 0.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        const QBrush textBrush(Qt::white);
        auto drawStrokedText = [&](const int x_, const int y_, const QString &text) {
            QPainterPath path;
            path.addText(x_, y_, font, text);
            painter.setPen(outlinePen);
            painter.setBrush(textBrush);
            painter.drawPath(path);
        };
        // tcas 判断
        const double _distance = distanceSimple(latitude, longitude, multiLatVal[0], multiLonVal[0]);
        const double _alt = std::abs(alt - multiAltVal[0]) * m2ft;
        bool notDisplay{false};
        switch (tcasMode) {
            case TcasMode::none:
                notDisplay = true;
                break;
            case TcasMode::nm30:
                if (_distance > nm2m * 30 || _alt > 9900)
                    notDisplay = true;
                break;
            case TcasMode::nm6:
                if (_distance > nm2m * 6 || _alt > 1200)
                    notDisplay = true;
                break;
            default:
                assert(false && "inop tcas mode");
        }
        if (notDisplay) {
            painter.restore();
            return;
        }
        // 航班信息
        QString flightId;
        flightId.reserve(7);
        for (int i = 8 * idx; i < 8 * (idx + 1) - 1; ++i)
            if (multiFlightIdVal[i] != 0)
                flightId.append(QChar(static_cast<char>(multiFlightIdVal[i])));
        // 高度信息
        int deltaAlt = static_cast<int>(std::round((alt - multiAltVal[0]) * m2ft / 100));
        QString altDescribe;
        if (deltaAlt >= 0) // 高度差
            altDescribe = QString::fromStdString(std::format("+{:02d}", deltaAlt));
        else
            altDescribe = QString::fromStdString(std::format("-{:02d}", -deltaAlt));
        if (vs >= 500) // 高度趋势
            altDescribe += "↑";
        else if (vs <= -500)
            altDescribe += "↓";
        else
            altDescribe += " ";
        drawStrokedText(10, 15, flightId);
        drawStrokedText(10, 25, altDescribe);
    }
    // 绘制飞机
    painter.rotate(trk);
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
 * @brief 更新机模的基本信息
 */
void PdfView::simuInfoUpdate () {
    if (!connected) // 未连接到模拟器
        return;
    connector->getDataref(multiId, multiIdVal, 0);
    connector->getDataref(multiLat, multiLatVal, 0);
    connector->getDataref(multiLon, multiLonVal, 0);
    connector->getDataref(multiAlt, multiAltVal, 0);
    connector->getDataref(multiTrk, multiTrkVal, 0);
    connector->getDataref(multiVs, multiVsVal, 0);
    connector->getDataref(multiFlightId, multiFlightIdVal, 0);
    // 设置更新
    SettingsManager &ins = SettingsManager::instance();
    ins.set(SettingsManager::latitu, static_cast<double>(multiLatVal[0]));
    ins.set(SettingsManager::longitu, static_cast<double>(multiLonVal[0]));
    ins.set(SettingsManager::altitu, static_cast<double>(multiAltVal[0]));
    // 映射不可用
    if (!transActive)
        return;
    // 不使用居中
    if (!centerOn || dragging) {
        viewport()->update();
        return;
    }

    // 自身居中逻辑
    auto [x,y] = trans(multiLatVal[0], multiLonVal[0]);
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

/**
 * @brief 初始化模拟器的一些东西
 */
void PdfView::simuInit () {
    constexpr int infoFreq = 1;
    // AI或多人
    multiId = connector->addDatarefArray("id", infoFreq);
    multiLat = connector->addDatarefArray("lat", infoFreq);
    multiLon = connector->addDatarefArray("lon", infoFreq);
    multiAlt = connector->addDatarefArray("alt", infoFreq);
    multiTrk = connector->addDatarefArray("trk", infoFreq);
    multiVs = connector->addDatarefArray("vs", infoFreq);
    multiFlightId = connector->addDatarefArray("flightId", infoFreq);
    // 回调
    connector->setCallback([this](const bool state) {
        setConnectState(state);
        qDebug() << "Simu-connect change state: " << state;
    });
}

void PdfView::setConnector (int value) {
    if (connector)
        connector->close();
    setConnectState(false);
    switch (static_cast<SimulatorSource>(value)) {
        case SimulatorSource::xplane:
            connector = std::make_unique<xpAdapter>();
            break;
        case SimulatorSource::wlan:
            connector = std::make_unique<wlanAdapter>();
            break;
        case SimulatorSource::real:
            connector = std::make_unique<realAdapter>();
            break;
        default:
            assert(false && "need to update switch case. [PdfView::setConnector]");
    }
    simuInit();
}

void PdfView::setConnectState (const bool state) {
    if (state == connected)
        return;
    connected = state;
    SettingsManager::instance().set(SettingsManager::simuConnect, state);
}
