#include "mapItemTemplate.hpp"

#include <QPolygonF>

void addVorHexagon (QPainterPath &path, const qreal size) {
    const qreal radius = size / 2.0;
    path.addPolygon(QPolygonF{
        QPointF{-radius / 2.0, -radius}, QPointF{radius / 2.0, -radius},
        QPointF{radius, 0.0}, QPointF{radius / 2.0, radius},
        QPointF{-radius / 2.0, radius}, QPointF{-radius, 0.0},
        QPointF{-radius / 2.0, -radius}
    });
}

void addDmeSquare (QPainterPath &path, const qreal size) {
    path.addRect(-size / 2.0, -size / 2.0, size, size);
}


QPainterPath airportSymbol (const qreal size) {
    QPainterPath path;
    path.addEllipse(QPointF{}, size / 2.0, size / 2.0);
    path.moveTo(0.0, -size / 2.0 + 2.0);
    path.lineTo(0.0, size / 2.0 - 2.0);
    return path;
}

QPainterPath fixSymbol (const qreal size) {
    const qreal radius = size / 2.0;
    QPainterPath path;
    path.addPolygon(QPolygonF{
        QPointF{0.0, -radius}, QPointF{radius, radius},
        QPointF{-radius, radius}, QPointF{0.0, -radius}
    });
    return path;
}

QPainterPath vorSymbol (const qreal size) {
    QPainterPath path;
    addVorHexagon(path, size);
    return path;
}

QPainterPath dmeSymbol (const qreal size) {
    QPainterPath path;
    addDmeSquare(path, size);
    return path;
}

QPainterPath vordmeSymbol (const qreal size) {
    QPainterPath path;
    addDmeSquare(path, size);
    addVorHexagon(path, size);
    return path;
}

QPainterPath ndbSymbol (const qreal size) {
    const qreal radius = size / 2.0;
    QPainterPath path;
    path.addEllipse(QPointF{}, radius, radius);
    path.addEllipse(QPointF{}, size / 3.0, size / 3.0);
    return path;
}
