#include "game2048.hpp"
#include "ui_note_widget.h"
#include "squareGameView.hpp"
#include <QFontMetrics>
#include <QKeyEvent>
#include <QPainter>
#include <QPixmap>
#include <QRandomGenerator>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <limits>

// Draw the whole board into one image. Cell geometry and text are independent
// of QLabel styling, font padding and layout minimum sizes.
class Game2048View : public SquareGameView {
    public:
        explicit Game2048View (QWidget *parent) : SquareGameView(parent) {
            setObjectName(QStringLiteral("game2048View"));
        }

        QSize sizeHint () const override { return {240, 240}; }
        QSize minimumSizeHint () const override { return {100, 100}; }

        void setBoard (const std::array<quint64, 16> &values) {
            board = values;
            update();
        }

    protected:
        void paintEvent (QPaintEvent *) override {
            const int side = std::min(width(), height());
            if (side <= 0)
                return;
            const qreal ratio = devicePixelRatioF();
            QPixmap image(QSize(static_cast<int>(std::ceil(side * ratio)),
                                static_cast<int>(std::ceil(side * ratio))));
            image.setDevicePixelRatio(ratio);
            image.fill(Qt::transparent);
            {
                QPainter painter(&image);
                painter.setRenderHint(QPainter::Antialiasing);
                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor("#bbada0"));
                painter.drawRoundedRect(QRectF(0, 0, side, side), side * 0.025, side * 0.025);

                static const std::array<const char*, 12> colors{
                    "#eee4da", "#ede0c8", "#f2b179", "#f59563", "#f67c5f", "#f65e3b",
                    "#edcf72", "#edcc61", "#edc850", "#edc53f", "#edc22e", "#3c3a32"
                };
                const qreal padding = side * 0.035;
                const qreal gap = side * 0.025;
                const qreal cellSide = (side - 2 * padding - 3 * gap) / 4;
                for (int i = 0; i < 16; ++i) {
                    const QRectF cell(padding + (i % 4) * (cellSide + gap),
                                      padding + (i / 4) * (cellSide + gap), cellSide, cellSide);
                    const auto value = board[i];
                    int color = 0;
                    for (auto level = value; level > 2 && color < 11; level /= 2)
                        ++color;
                    painter.setPen(Qt::NoPen);
                    painter.setBrush(QColor(value == 0 ? "#cdc1b4" : colors[color]));
                    painter.drawRoundedRect(cell, cellSide * 0.06, cellSide * 0.06);
                    if (value == 0)
                        continue;

                    const auto text = QString::number(value);
                    auto tileFont = font();
                    tileFont.setBold(true);
                    const int pixels = std::max(1, static_cast<int>(std::round(cellSide * 0.42)));
                    tileFont.setPixelSize(pixels);
                    const QFontMetricsF metrics(tileFont);
                    const qreal fit = std::min({1.0, cellSide * 0.82 / metrics.horizontalAdvance(text),
                                               cellSide * 0.75 / metrics.height()});
                    tileFont.setPixelSize(std::max(1, static_cast<int>(std::floor(pixels * fit))));
                    painter.setFont(tileFont);
                    painter.setPen(QColor(value <= 4 ? "#776e65" : "#ffffff"));
                    painter.drawText(cell, Qt::AlignCenter, text);
                }
            }
            QPainter painter(this);
            painter.drawPixmap(QPointF((width() - side) / 2.0, 0), image);
        }

    private:
        std::array<quint64, 16> board{};
};

namespace {
bool canMerge2048 (quint64 first, quint64 second) {
    return first != 0 && first == second && first <= std::numeric_limits<quint64>::max() / 2;
}

bool canMove2048 (const std::array<quint64, 16> &board) {
    for (int i = 0; i < 16; ++i)
        if (board[i] == 0 || (i % 4 < 3 && canMerge2048(board[i], board[i + 1]))
            || (i / 4 < 3 && canMerge2048(board[i], board[i + 4])))
            return true;
    return false;
}
}

Game2048::Game2048 (Ui::note_widget *ui, QObject *parent) : Game(ui, parent) {
    init2048();
}

void Game2048::init2048 () {
    for (auto *label : {ui->game2048Rules, ui->game2048Score, ui->game2048Feedback})
        label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    game2048View = new Game2048View(ui->game2048Board);
    ui->game2048Grid->addWidget(game2048View, 0, 0);
    const std::array<std::pair<QToolButton*, Qt::Key>, 4> directions{{
        {ui->game2048LeftButton, Qt::Key_Left}, {ui->game2048UpButton, Qt::Key_Up},
        {ui->game2048DownButton, Qt::Key_Down}, {ui->game2048RightButton, Qt::Key_Right}
    }};
    for (const auto &[button, direction] : directions) {
        button->setAccessibleName(button->toolTip());
        connect(button, &QToolButton::clicked, this, [this, direction] { move2048(direction); });
    }
    connect(ui->game2048NewButton, &QToolButton::clicked, this, &Game2048::reset2048);
    ui->game2048Board->setAccessibleName(tr("2048棋盘，使用方向键或WASD移动"));
    ui->page2048->installEventFilter(this);
    for (auto *widget : ui->page2048->findChildren<QWidget*>())
        widget->installEventFilter(this);
    reset2048();
}

void Game2048::reset2048 () {
    game2048Board.fill(0);
    game2048Score = 0;
    spawn2048Tile();
    spawn2048Tile();
    update2048();
    if (ui->page2048->isVisible())
        ui->game2048Board->setFocus(Qt::OtherFocusReason);
}

void Game2048::spawn2048Tile () {
    std::array<int, 16> empty{};
    int count = 0;
    for (int i = 0; i < 16; ++i)
        if (game2048Board[i] == 0)
            empty[count++] = i;
    if (count == 0)
        return;
    auto *random = QRandomGenerator::global();
    game2048Board[empty[random->bounded(count)]] = random->bounded(10) == 0 ? 4 : 2;
}

void Game2048::move2048 (Qt::Key direction) {
    auto next = game2048Board;
    quint64 gained = 0;
    for (int line = 0; line < 4; ++line) {
        // Read each row/column from the destination edge. Compact first, then
        // merge pairs once; a newly merged tile cannot merge again in this move.
        const auto index = [line, direction](int offset) {
            switch (direction) {
                case Qt::Key_Left: return line * 4 + offset;
                case Qt::Key_Right: return line * 4 + 3 - offset;
                case Qt::Key_Up: return offset * 4 + line;
                case Qt::Key_Down: return (3 - offset) * 4 + line;
                default: return -1;
            }
        };
        if (index(0) < 0)
            return;
        std::array<quint64, 4> compact{};
        int count = 0;
        for (int offset = 0; offset < 4; ++offset)
            if (const auto value = game2048Board[index(offset)]; value != 0)
                compact[count++] = value;
        std::array<quint64, 4> merged{};
        int output = 0;
        for (int i = 0; i < count; ++i) {
            auto value = compact[i];
            if (i + 1 < count && canMerge2048(value, compact[i + 1])) {
                value *= 2;
                gained += value;
                ++i;
            }
            merged[output++] = value;
        }
        for (int offset = 0; offset < 4; ++offset)
            next[index(offset)] = merged[offset];
    }
    if (next != game2048Board) {
        game2048Board = next;
        game2048Score += gained;
        spawn2048Tile();
    }
    update2048();
    ui->game2048Board->setFocus(Qt::OtherFocusReason);
}

void Game2048::update2048 () const {
    game2048View->setBoard(game2048Board);
    QStringList rows;
    for (int row = 0; row < 4; ++row) {
        QStringList values;
        for (int column = 0; column < 4; ++column) {
            const auto value = game2048Board[row * 4 + column];
            values.append(value == 0 ? tr("空") : QString::number(value));
        }
        rows.append(tr("第%1行：%2").arg(row + 1).arg(values.join(QStringLiteral("、"))));
    }
    ui->game2048Board->setAccessibleDescription(rows.join(QStringLiteral("；")));
    ui->game2048Score->setText(tr("得分：%1").arg(game2048Score));
    const bool available = canMove2048(game2048Board);
    const auto maximum = *std::ranges::max_element(game2048Board);
    for (auto *button : {ui->game2048LeftButton, ui->game2048UpButton,
                        ui->game2048DownButton, ui->game2048RightButton})
        button->setEnabled(available);
    ui->game2048Feedback->setText(!available
        ? (maximum >= 2048 ? tr("已达成2048！没有可移动的格子了，点击新一局再挑战。")
                           : tr("没有可移动的格子了，点击新一局再挑战。"))
        : (maximum >= 2048 ? tr("达成2048！可以继续挑战更大的数字。") : tr("合并数字，向2048前进！")));
}

bool Game2048::eventFilter (QObject *watched, QEvent *event) {
    if (watched == ui->page2048 && event->type() == QEvent::Show) {
        QTimer::singleShot(0, this, [this] {
            if (ui->page2048->isVisible())
                ui->game2048Board->setFocus(Qt::OtherFocusReason);
        });
    }
    auto *widget = qobject_cast<QWidget*>(watched);
    if (widget && (widget == ui->page2048 || ui->page2048->isAncestorOf(widget))
        && ui->page2048->isVisible()) {
        if (event->type() == QEvent::MouseButtonPress)
            ui->game2048Board->setFocus(Qt::MouseFocusReason);
        if (event->type() == QEvent::KeyPress) {
            const auto *key = static_cast<QKeyEvent*>(event);
            if ((key->modifiers() & ~Qt::KeypadModifier) == Qt::NoModifier) {
                switch (key->key()) {
                    case Qt::Key_Left: case Qt::Key_A: move2048(Qt::Key_Left); return true;
                    case Qt::Key_Right: case Qt::Key_D: move2048(Qt::Key_Right); return true;
                    case Qt::Key_Up: case Qt::Key_W: move2048(Qt::Key_Up); return true;
                    case Qt::Key_Down: case Qt::Key_S: move2048(Qt::Key_Down); return true;
                    default: break;
                }
            }
        }
    }
    return QObject::eventFilter(watched, event);
}
