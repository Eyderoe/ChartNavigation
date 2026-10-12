#ifndef CHARTNAVIGATION_GAMES_GRIDGAMEVIEW_HPP
#define CHARTNAVIGATION_GAMES_GRIDGAMEVIEW_HPP

#include "squareGameView.hpp"
#include <QPainter>
#include <algorithm>

namespace GameGrid {
// Directions run clockwise from north. Edges do not wrap.
inline int neighbor (int cell, int direction, int side) {
    if (cell < 0 || cell >= side * side) return -1;
    switch (direction) {
        case 0: return cell >= side ? cell - side : -1;
        case 1: return cell % side + 1 < side ? cell + 1 : -1;
        case 2: return cell + side < side * side ? cell + side : -1;
        case 3: return cell % side > 0 ? cell - 1 : -1;
        default: return -1;
    }
}
}

class GridGameView : public SquareGameView {
    public:
        explicit GridGameView (QWidget *parent) : SquareGameView(parent) {
            setFocusPolicy(Qt::StrongFocus);
        }
        QSize sizeHint () const override { return {280, 280}; }
        QSize minimumSizeHint () const override { return {100, 100}; }
        virtual QRectF boardRect () const {
            const qreal size = std::min(width(), height());
            return QRectF((width() - size) / 2, 0, size, size).adjusted(4, 4, -4, -4);
        }
        QRectF cellRect (int cell) const {
            const auto rect = boardRect();
            const qreal size = rect.width() / gridSide;
            return {rect.left() + cell % gridSide * size, rect.top() + cell / gridSide * size, size, size};
        }
        int cellAt (QPointF point) const {
            const auto rect = boardRect();
            if (gridSide <= 0 || rect.width() <= 0 || !rect.contains(point)) return -1;
            const int x = (point.x() - rect.left()) * gridSide / rect.width();
            const int y = (point.y() - rect.top()) * gridSide / rect.height();
            return x < gridSide && y < gridSide ? y * gridSide + x : -1;
        }
    protected:
        int gridSide{};
        void drawCursor (QPainter &painter, int cell) const {
            if (!hasFocus() || cell < 0) return;
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(palette().color(QPalette::Text), 2, Qt::DashLine));
            painter.drawRect(cellRect(cell).adjusted(3, 3, -3, -3));
        }
        void focusInEvent (QFocusEvent *event) override {
            SquareGameView::focusInEvent(event);
            update();
        }
        void focusOutEvent (QFocusEvent *event) override {
            SquareGameView::focusOutEvent(event);
            update();
        }
};

#endif // CHARTNAVIGATION_GAMES_GRIDGAMEVIEW_HPP
