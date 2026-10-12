#include "lightsOut.hpp"
#include "ui_note_widget.h"
#include "services/settingManage.hpp"
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QRandomGenerator>
#include <QStringList>
#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>

namespace {
constexpr int boardSide = 5;

void toggleLightsOut (std::array<bool, 25> &board, int index) {
    const int row = index / 5;
    const int column = index % 5;
    board[index] = !board[index];
    if (row > 0) board[index - 5] = !board[index - 5];
    if (row < 4) board[index + 5] = !board[index + 5];
    if (column > 0) board[index - 1] = !board[index - 1];
    if (column < 4) board[index + 1] = !board[index + 1];
}

QRectF lightsOutCellRect (qreal side, int index) {
    const qreal padding = side * 0.035;
    const qreal gap = side * 0.025;
    const qreal cellSide = (side - 2 * padding - 4 * gap) / boardSide;
    return {padding + (index % boardSide) * (cellSide + gap),
            padding + (index / boardSide) * (cellSide + gap), cellSide, cellSide};
}
}

class LightsOutView : public QWidget {
    public:
        explicit LightsOutView (QWidget *parent) : QWidget(parent) {
            setObjectName(QStringLiteral("lightsOutView"));
            QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            policy.setHeightForWidth(true);
            setSizePolicy(policy);
            setMaximumHeight(200);
            setMinimumSize(100, 100);
            setFocusPolicy(Qt::StrongFocus);
            setAccessibleName(QStringLiteral("关灯游戏棋盘"));
            connect(&SettingsManager::instance(),
                    qOverload<SettingsManager::TempKey, const QVariant&>(&SettingsManager::settingChanged),
                    this, [this](SettingsManager::TempKey key, const QVariant &) {
                        if (key == SettingsManager::isDarkTheme)
                            update();
                    });
        }

        QSize sizeHint () const override { return {200, 200}; }
        QSize minimumSizeHint () const override { return {100, 100}; }
        int heightForWidth (int width) const override { return std::min(width, 200); }

        void setBoard (const std::array<bool, 25> &state, bool active) {
            board = state;
            enabled = active;
            updateAccessibleText();
            update();
        }

        void setCellActivated (std::function<void(int)> handler) {
            cellActivated = std::move(handler);
        }

    protected:
        void paintEvent (QPaintEvent *) override {
            const qreal side = std::min(width(), height());
            if (side <= 0)
                return;

            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing);
            const QPointF origin((width() - side) / 2.0, (height() - side) / 2.0);
            painter.translate(origin);

            const bool dark = SettingsManager::instance().get(SettingsManager::isDarkTheme).toBool();
            const QColor boardColor(dark ? QStringLiteral("#27343F") : QStringLiteral("#E4E8EC"));
            const QColor offCell(dark ? QStringLiteral("#394956") : QStringLiteral("#F8FAFB"));
            const QColor offLamp(dark ? QStringLiteral("#536675") : QStringLiteral("#DCE2E7"));
            const QColor offStroke(dark ? QStringLiteral("#8193A0") : QStringLiteral("#9DA9B2"));
            const QColor litCell(dark ? QStringLiteral("#D3A342") : QStringLiteral("#F0C15D"));
            const QColor litLamp(dark ? QStringLiteral("#FFF0C4") : QStringLiteral("#FFF7DF"));
            const QColor litStroke(dark ? QStringLiteral("#F0D18B") : QStringLiteral("#D4A03C"));
            const QColor focusColor(dark ? QStringLiteral("#84C7FF") : QStringLiteral("#1876D2"));
            painter.setPen(Qt::NoPen);
            painter.setBrush(boardColor);
            painter.drawRoundedRect(QRectF(0, 0, side, side), side * 0.035, side * 0.035);

            for (int index = 0; index < static_cast<int>(board.size()); ++index) {
                const auto cell = lightsOutCellRect(side, index);
                const bool lit = board[index];
                const bool selected = enabled && index == selectedCell && hasFocus();
                painter.setBrush(lit ? litCell : offCell);
                painter.setPen(QPen(selected ? focusColor : (lit ? litStroke : offStroke),
                                    std::max<qreal>(1.0, side * (selected ? 0.012 : 0.004))));
                painter.drawRoundedRect(cell, side * 0.018, side * 0.018);

                const qreal lampDiameter = cell.width() * 0.42;
                const QRectF lamp(cell.center().x() - lampDiameter / 2.0,
                                  cell.center().y() - lampDiameter / 2.0,
                                  lampDiameter, lampDiameter);
                painter.setBrush(lit ? litLamp : offLamp);
                painter.setPen(QPen(lit ? litStroke : offStroke,
                                    std::max<qreal>(1.0, side * 0.006)));
                painter.drawEllipse(lamp);
            }
        }

        void mousePressEvent (QMouseEvent *event) override {
            if (event->button() == Qt::LeftButton && enabled) {
                setFocus(Qt::MouseFocusReason);
                const qreal side = std::min(width(), height());
                const QPointF point = event->position()
                    - QPointF((width() - side) / 2.0, (height() - side) / 2.0);
                for (int index = 0; index < static_cast<int>(board.size()); ++index) {
                    if (lightsOutCellRect(side, index).contains(point)) {
                        selectedCell = index;
                        updateAccessibleText();
                        if (cellActivated)
                            cellActivated(index);
                        update();
                        event->accept();
                        return;
                    }
                }
            }
            QWidget::mousePressEvent(event);
        }

        void keyPressEvent (QKeyEvent *event) override {
            int next = selectedCell;
            switch (event->key()) {
                case Qt::Key_Left: case Qt::Key_A: next = selectedCell % boardSide > 0 ? selectedCell - 1 : selectedCell; break;
                case Qt::Key_Right: case Qt::Key_D: next = selectedCell % boardSide + 1 < boardSide ? selectedCell + 1 : selectedCell; break;
                case Qt::Key_Up: case Qt::Key_W: next = selectedCell >= boardSide ? selectedCell - boardSide : selectedCell; break;
                case Qt::Key_Down: case Qt::Key_S: next = selectedCell + boardSide < board.size() ? selectedCell + boardSide : selectedCell; break;
                case Qt::Key_Space: case Qt::Key_Return: case Qt::Key_Enter:
                    if (enabled && cellActivated)
                        cellActivated(selectedCell);
                    event->accept();
                    return;
                default: QWidget::keyPressEvent(event); return;
            }
            if (next != selectedCell) {
                selectedCell = next;
                updateAccessibleText();
                update();
            }
            event->accept();
        }

        void focusInEvent (QFocusEvent *event) override {
            QWidget::focusInEvent(event);
            update();
        }

        void focusOutEvent (QFocusEvent *event) override {
            QWidget::focusOutEvent(event);
            update();
        }

    private:
        std::array<bool, 25> board{};
        int selectedCell{};
        bool enabled{true};
        std::function<void(int)> cellActivated;

        void updateAccessibleText () {
            QStringList rows;
            for (int row = 0; row < boardSide; ++row) {
                QStringList cells;
                for (int column = 0; column < boardSide; ++column)
                    cells.append(board[row * boardSide + column] ? tr("亮") : tr("灭"));
                rows.append(tr("第%1行：%2").arg(row + 1).arg(cells.join(QStringLiteral("、"))));
            }
            setAccessibleName(tr("关灯棋盘，第%1行，第%2列，%3")
                .arg(selectedCell / boardSide + 1).arg(selectedCell % boardSide + 1)
                .arg(board[selectedCell] ? tr("亮") : tr("灭")));
            setAccessibleDescription(rows.join(QStringLiteral("；")));
        }

};

LightsOutGame::LightsOutGame (Ui::note_widget *ui, QObject *parent) : Game(ui, parent) {
    initLightsOut();
}

void LightsOutGame::initLightsOut () {
    for (auto *label : {ui->lightsOutRules, ui->lightsOutFeedback}) {
        label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    }
    updateLightsOutTheme();
    connect(&SettingsManager::instance(),
            qOverload<SettingsManager::TempKey, const QVariant&>(&SettingsManager::settingChanged),
            this, [this](SettingsManager::TempKey key, const QVariant &) {
                if (key == SettingsManager::isDarkTheme)
                    updateLightsOutTheme();
            });
    lightsOutView = new LightsOutView(ui->lightsOutBoard);
    ui->lightsOutBoardLayout->addWidget(lightsOutView);
    lightsOutView->setCellActivated([this](int index) {
        if (std::ranges::none_of(lightsOutBoard, [](bool lit) { return lit; }))
            return;
        toggleLightsOut(lightsOutBoard, index);
        ++lightsOutMoves;
        updateLightsOut();
    });
    connect(ui->lightsOutNewButton, &QToolButton::clicked, this, &LightsOutGame::resetLightsOut);
    connect(ui->lightsOutRetryButton, &QToolButton::clicked, this, [this] {
        lightsOutBoard = lightsOutInitialBoard;
        lightsOutMoves = 0;
        updateLightsOut();
    });
    resetLightsOut();
}

void LightsOutGame::updateLightsOutTheme () const {
    const bool dark = SettingsManager::instance().get(SettingsManager::isDarkTheme).toBool();
    const QString foreground = dark ? QStringLiteral("#DFE1E2") : QStringLiteral("#19232D");
    const QString background = dark ? QStringLiteral("#19232D") : QStringLiteral("#FAFAFA");
    const QString style = QStringLiteral("background-color: transparent; border: none; padding: 0; color: %1;")
        .arg(foreground);
    ui->pageLightsOut->setStyleSheet(QStringLiteral("QWidget#pageLightsOut { background-color: %1; }")
        .arg(background));
    ui->lightsOutRules->setStyleSheet(style);
    ui->lightsOutFeedback->setStyleSheet(style);
}

void LightsOutGame::resetLightsOut () {
    // Scramble a solved board using legal moves, so reversing them always solves it.
    do {
        lightsOutBoard.fill(false);
        for (int i = 0; i < 25; ++i)
            if (QRandomGenerator::global()->bounded(2))
                toggleLightsOut(lightsOutBoard, i);
    } while (std::ranges::none_of(lightsOutBoard, [](bool lit) { return lit; }));
    lightsOutInitialBoard = lightsOutBoard;
    lightsOutMoves = 0;
    updateLightsOut();
}

void LightsOutGame::updateLightsOut () const {
    const auto remaining = std::ranges::count(lightsOutBoard, true);
    lightsOutView->setBoard(lightsOutBoard, remaining != 0);
    ui->lightsOutFeedback->setText(remaining == 0
        ? tr("全部熄灭！共用了 %1 步，点击新一局继续。").arg(lightsOutMoves)
        : tr("步数：%1　剩余亮灯：%2").arg(lightsOutMoves).arg(remaining));
}

