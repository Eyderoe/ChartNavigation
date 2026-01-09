#include "pdfView.hpp"
#include "tools/stringProcess.hpp"
#include "tools/constValue.hpp"

PdfView::PdfView (QWidget *parent) : QPdfView(parent) {
    setPageMode(PageMode::SinglePage);
    setZoomMode(ZoomMode::Custom);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // 地图绘制
    plane.load(":/map/resources/plane_small.png");
    otherPlane.load(":/map/resources/plane_small_2.png");
    // xplane
    xpInit();
    // 定时器
    const QSettings settings;
    const int centerFreq = settings.value("center_freq", 1).toInt();
    xpUpdateTimer.setInterval(1000 / centerFreq);
    connect(&xpUpdateTimer, &QTimer::timeout, this, &PdfView::xpInfoUpdate);
    xpUpdateTimer.start();
}

/**
 * @brief 获取PDF文档某一页尺寸
 * @param page 页数
 * @return (长,宽) 单位:点(1/72英寸)
 */
QSizeF PdfView::getDocSize (const int page) const {
    return document()->pagePointSize(page);
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
    rotate = rotateDegree;
    transActive = transformer.loadData(data, threshold);
    if (!transActive)
        return;
    auto [error,errors] = transformer.evaluate();
    // Debug输出
    auto view = errors | std::views::transform([](double num) { return std::format("{:.2f}", num); });
    qDebug() << std::format("RMS: {:.2f}, errors: [{}]", error, join(view, ", "));
}

void PdfView::closeXp () {
    xp.close();
}

void PdfView::wheelEvent (QWheelEvent *event) {
    const double oldZoom = zoomFactor();
    const QPointF mousePos = event->position();
    const double logicX = (horizontalScrollBar()->value() + mousePos.x()) / oldZoom;
    const double logicY = (verticalScrollBar()->value() + mousePos.y()) / oldZoom;

    double newZoom = oldZoom;
    if (event->angleDelta().y() > 0)
        newZoom *= 1.2;
    else
        newZoom *= 0.8;
    newZoom = qBound(0.2, newZoom, 4.0);

    setZoomFactor(newZoom);
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
        // 航班信息
        QString flightId;
        flightId.reserve(7);
        for (int i = 8 * idx; i < 8 * (idx + 1) - 1; ++i)
            if (multiFlightIdVal[i] != 0)
                flightId.append(QChar(static_cast<char>(multiFlightIdVal[i])));
        // 高度信息
        int deltaAlt = static_cast<int>(std::round((alt - multiAltVal[0]) * m2ft / 100));
        QString delta;
        if (deltaAlt >= 0)
            delta = QString::fromStdString(std::format("{:02d}", deltaAlt));
        else
            delta = QString::fromStdString(std::format("-{:02d}", -deltaAlt));
        if (vs >= 500)
            delta += "↑";
        else if (vs <= -500)
            delta += "↓";
        drawStrokedText(10, 15, flightId);
        if (std::abs(deltaAlt) >= 2)
            drawStrokedText(10, 25, delta);
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
void PdfView::xpInfoUpdate () {
    if (!connected || !transActive) {
        viewport()->update();
        return;
    }
    xp.getDataref(multiId, multiIdVal);
    xp.getDataref(multiLat, multiLatVal);
    xp.getDataref(multiLon, multiLonVal);
    xp.getDataref(multiAlt, multiAltVal);
    xp.getDataref(multiTrk, multiTrkVal);
    xp.getDataref(multiVs, multiVsVal);
    xp.getDataref(multiFlightId, multiFlightIdVal);
    if (!centerOn || dragging) {
        viewport()->update();
        return;
    }

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
 * @brief 初始化xp的一些东西
 */
void PdfView::xpInit () {
    const QSettings settings;
    const int xpFreq = settings.value("xp_freq", 1).toInt();
    // AI或多人
    multiId = xp.addDatarefArray("sim/cockpit2/tcas/targets/modeS_id", 64, xpFreq);
    multiLat = xp.addDatarefArray("sim/cockpit2/tcas/targets/position/lat", 64, xpFreq);
    multiLon = xp.addDatarefArray("sim/cockpit2/tcas/targets/position/lon", 64, xpFreq);
    multiAlt = xp.addDatarefArray("sim/cockpit2/tcas/targets/position/ele", 64, xpFreq);
    multiTrk = xp.addDatarefArray("sim/cockpit2/tcas/targets/position/psi", 64, xpFreq);
    multiVs = xp.addDatarefArray("sim/cockpit2/tcas/targets/position/vertical_speed", 64, xpFreq);
    multiFlightId = xp.addDatarefArray("sim/cockpit2/tcas/targets/flight_id", 512, xpFreq);
    // 回调
    xp.setCallback([this](const bool state) {
        this->connected = state;
        qDebug() << "XPlane change state: " << state;
    });
}
