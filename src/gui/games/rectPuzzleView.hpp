#ifndef CHARTNAVIGATION_GAMES_RECTPUZZLEVIEW_HPP
#define CHARTNAVIGATION_GAMES_RECTPUZZLEVIEW_HPP
#include "squareGameView.hpp"
#include <QPointF>
#include <algorithm>
#include <cmath>

class RectPuzzleView : public SquareGameView {
  public:
    explicit RectPuzzleView(QWidget *parent) : SquareGameView(parent) { setFocusPolicy(Qt::StrongFocus); }
    QSize sizeHint() const override { return {320, 320}; }
    QSize minimumSizeHint() const override { return {100, 100}; }
    QRectF boardRect() const {
        const qreal tile = std::max(0.0, std::min((width() - 16.0) / columns, (height() - 16.0) / rows));
        return {(width() - tile * columns) / 2, (height() - tile * rows) / 2, tile * columns, tile * rows};
    }
    QPointF cellCentre(int cell) const {
        const auto rect = boardRect();
        const qreal tile = rect.width() / columns;
        return rect.topLeft() + QPointF((cell % columns + 0.5) * tile, (cell / columns + 0.5) * tile);
    }
    QPoint canonical(QPointF point) const {
        const auto rect = boardRect();
        const qreal tile = rect.width() / columns;
        if (tile <= 0)
            return {-100, -100};
        return {static_cast<int>(std::floor((point.x() - rect.left()) * 100 / tile + 50)),
                static_cast<int>(std::floor((point.y() - rect.top()) * 100 / tile + 50))};
    }

  protected:
    int columns{9}, rows{9};
};
#endif
