#include "pdfView.hpp"

PdfView::PdfView (QWidget *parent) : QPdfView(parent) {
    setPageMode(PageMode::SinglePage);
    setZoomMode(ZoomMode::Custom);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // 地图绘制
    plane.load(":/map/resources/plane_small.png");
    // xplane
    xp.addPlaneInfo();
    xp.setCallback([this](const bool state) {
        this->connected = state;
        qDebug() << "XPlane change state: " << state;
    });
    // 定时器
    xpUpdateTimer.setInterval(1000);
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
 */
void PdfView::loadMappingData (const std::vector<std::vector<double>> &data, const double rotateDegree) {
    rotate = rotateDegree;
    transActive = transformer.loadData(data);
    if (!transActive)
        return;
    auto [error,_] = transformer.evaluate();
    qDebug() << std::format("RMS error: {:.2f}", error);
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
    if (planeInfo.track == -999) // xp信息不可用
        check = false;
    // 飞机绘制逻辑
    if (check) {
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        auto [x,y] = trans(planeInfo.lat, planeInfo.lon);
        painter.save();
        painter.translate(x, y);
        const double direct = std::fmod(planeInfo.track + rotate + 360, 360);
        painter.rotate(direct);
        painter.scale(0.4, 0.4);
        painter.drawPixmap(-plane.width() / 2, -plane.height() / 2, plane);
        painter.restore();
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
    if (connected) {
        xp.getPlaneInfo(planeInfo);
        if (centerOn && !dragging) {
            auto [x,y] = trans(planeInfo.lat, planeInfo.lon);
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
        }
    } else {
        planeInfo.track = -999;
    }
    viewport()->update();
}
