#include "pdfView.hpp"

#include <iostream>

PdfView::PdfView (QWidget *parent) : QGraphicsView(parent) {
    // TODO 修改基类后炸了
    setTransformationAnchor(AnchorUnderMouse);
    setResizeAnchor(AnchorUnderMouse);
    setDragMode(ScrollHandDrag);
    // 地图绘制
    scene = new QGraphicsScene(this);
    setScene(scene);
    plane.load(":/map/resources/plane_small.png");
    connect(&render, &QPdfPageRenderer::pageRendered, this, &PdfView::renderComplete);
    // xplane
    xp.addPlaneInfo();
    xp.setCallback([this](const bool state) {
        this->connected = state;
        qDebug() << "XPlane change state: " << state;
    });
    connect(&timer, &QTimer::timeout, this, &PdfView::xpInfoUpdate);
    timer.setInterval(1000);
    timer.start();
}

void PdfView::loadMappingData (const std::vector<std::vector<double>> &data) {
    transActive = transformer.loadData(data);
    if (!transActive)
        return;
    auto [error,_] = transformer.evaluate();
    qDebug() << std::format("navi available! error: {:.2f}", error);
}

void PdfView::setPdf (QPdfDocument *file) {
    scene->clear();
    if (file == nullptr)
        return;
    // 计算尺寸
    docSize = file->pagePointSize(0);
    constexpr qreal dpi = 400;
    const QSize imageSize = (docSize * dpi / 72.0).toSize();
    // 渲染
    render.setDocument(file);
    QImage image(imageSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    render.requestPage(0, imageSize);
}

void PdfView::renderComplete (int pageNumber, QSize imageSize, const QImage &image, QPdfDocumentRenderOptions options) {
    auto pic = QPixmap::fromImage(image);
    auto *pageItem = scene->addPixmap(pic);
    scene->setSceneRect(pageItem->boundingRect());
}

void PdfView::wheelEvent (QWheelEvent *event) {
    constexpr double scaleFactor = 1.15;
    const double factor = (event->angleDelta().y() > 0) ? scaleFactor : 1.0 / scaleFactor;
    const double currentScale = transform().m11();
    const double newScale = currentScale * factor;
    if (newScale < 0.2 || newScale > 4.0)
        return;
    scale(factor, factor);
}

std::pair<double, double> PdfView::trans () {
    // TODO 转换逻辑没有
    return {0, 0};
}

void PdfView::paintEvent (QPaintEvent *event) {
    // TODO 绘制逻辑没有
    QGraphicsView::paintEvent(event);
}


void PdfView::xpInfoUpdate () {
    if (connected)
        xp.getPlaneInfo(planeInfo);
    else
        planeInfo.track = -999;
    this->viewport()->update();
}
