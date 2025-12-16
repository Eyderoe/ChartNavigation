#include "pdfView.hpp"

#include <iostream>

PdfView::PdfView (QWidget *parent) : QPdfView(parent) {
    setPageMode(PageMode::SinglePage);
    setZoomMode(ZoomMode::Custom);
    // setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // 地图绘制
    plane.load(":/map/resources/plane_small.png");
    // xplane
    xp.addPlaneInfo();
    xp.setCallback([this](const bool state) {
        this->connected = state;
        qDebug() << "XPlane change state: " << state;
    });
    timer.setInterval(1000);
    connect(&timer, &QTimer::timeout, this, &PdfView::xpInfoUpdate);
    timer.start();
}

void PdfView::setDocSize (const QSizeF point) {
    docSize = point;
}

void PdfView::loadMappingData (const std::vector<std::vector<double>> &data) {
    transActive = transformer.loadData(data);
    if (!transActive)
        return;
    auto [error,_] = transformer.evaluate();
    qDebug() << std::format("nav available! error: {:.2f}", error);
}

void PdfView::wheelEvent (QWheelEvent *event) {
    const double oldZoom = zoomFactor();
    const QPointF mousePos = event->position();
    const double logicX = (horizontalScrollBar()->value() + mousePos.x()) / oldZoom;
    const double logicY = (verticalScrollBar()->value() + mousePos.y()) / oldZoom;

    double newZoom = oldZoom;
    if (event->angleDelta().y() > 0)
        newZoom *= 1.1;
    else
        newZoom *= 0.9;
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

std::pair<double, double> PdfView::trans () {
    // 获取文档位置
    auto [x, y] = transformer.transform(planeInfo.lat, planeInfo.lon);
    // 获取基本信息
    const auto viewSize = viewport()->size();
    const auto *scree = screen();
    const auto vertBar = verticalScrollBar(), horzBar = horizontalScrollBar();
    const auto dpi = scree->logicalDotsPerInch();
    const auto logicDocSize = zoomFactor() * docSize * dpi / 72;
    const auto margin = documentMargins();
    // x计算
    double finalX{};
    const double scaleX = x / docSize.width(); // 1 PDF (点)
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
    const double scaleY = y / docSize.width();
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

void PdfView::paintEvent (QPaintEvent *event) {
    trans();
    QPdfView::paintEvent(event);
    if (!connected) // xp已连接
        return;
    if (plane.isNull()) // 图片不可用
        return;
    if (!transActive) // 仿射变换可用
        return;
    if (planeInfo.track == -999) // xp信息不可用
        return;
    QPainter painter(viewport());
    if (!painter.isActive()) // PDF文档已加载
        return;

    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    auto [x,y] = trans();
    painter.save();
    painter.translate(x, y);
    const double direct = std::fmod(planeInfo.track + 360, 360);
    painter.rotate(direct);
    painter.scale(0.4, 0.4);
    painter.drawPixmap(-plane.width() / 2.0, -plane.height() / 2.0, plane);
    painter.restore();
}

void PdfView::xpInfoUpdate () {
    if (connected)
        xp.getPlaneInfo(planeInfo);
    else
        planeInfo.track = -999;
    this->viewport()->update();
}
