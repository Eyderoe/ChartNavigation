#include "enhancedMap.hpp"

#include <QGraphicsRectItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsTextItem>
#include <QBrush>
#include <QPen>

MapView::MapView(QWidget *parent) : QGraphicsView(parent) {
    scene = new QGraphicsScene(this);
    setScene(scene);

    // 矩形
    auto *rect = scene->addRect(20, 20, 200, 120, QPen(Qt::blue, 2), QBrush(Qt::cyan));
    rect->setFlag(QGraphicsItem::ItemIsMovable, true);

    // 椭圆
    auto *ellipse = scene->addEllipse(150, 80, 160, 100, QPen(Qt::red, 2), QBrush(QColor(255, 170, 0, 160)));
    ellipse->setFlag(QGraphicsItem::ItemIsMovable, true);

    // 文字
    auto *text = scene->addText("Dev. Developing!");
    text->setDefaultTextColor(Qt::darkGreen);
    text->setPos(40, 160);
    text->setFlag(QGraphicsItem::ItemIsMovable, true);

    setRenderHint(QPainter::Antialiasing);
    setDragMode(QGraphicsView::ScrollHandDrag);
}
