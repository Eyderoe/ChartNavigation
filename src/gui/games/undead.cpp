#include "undead.hpp"
#include "rectPuzzleView.hpp"
#include "ui_note_widget.h"
#include "ui/theme.hpp"
#include <QFutureWatcher>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRandomGenerator>
#include <QSignalBlocker>
#include <QtConcurrent/QtConcurrentRun>
#include <array>
class UndeadView : public RectPuzzleView {
  public:
    explicit UndeadView(QWidget *parent) : RectPuzzleView(parent) {
        setObjectName("undeadView");
        setAccessibleName(tr("不死族棋盘"));
    }
    void setState(const UndeadBackend *s, const UndeadInput *i, UndeadConfig c) {
        state = s;
        input = i;
        config = c;
        columns = c.width + 2;
        rows = c.height + 2;
        update();
    }
    QPointF gridPoint(int cell) const {
        return cellCentre(cell % config.width + 1 + (cell / config.width + 1) * columns);
    }
    QPoint inputPoint(QPointF point) const {
        const auto rect = boardRect();
        qreal t = rect.width() / columns;
        if (t <= 0)
            return {-100, -100};
        return {int(std::floor((point.x() - rect.left()) * 100 / t + 26)),
                int(std::floor((point.y() - rect.top()) * 100 / t + 127))};
    }

  protected:
    void drawMonster(QPainter &p, QRectF box, int value, bool ascii, bool bad) {
        QColor ink = bad ? QColor("#e46b78") : palette().color(QPalette::Text);
        p.setPen(QPen(ink, std::max(.8, box.width() * .03)));
        auto c = box.center();
        qreal t = box.width();
        if (ascii) {
            auto f = font();
            f.setPixelSize(std::max(1, int(t * .65)));
            p.setFont(f);
            p.drawText(box, Qt::AlignCenter, value == 1 ? "G" : value == 2 ? "V" : "Z");
            return;
        }
        if (value == 1) {
            p.setBrush(bad ? QColor("#e46b78") : QColor("#b7bce7"));
            QPainterPath shape;
            shape.moveTo(c + QPointF(-t * .32, t * .3));
            shape.lineTo(c + QPointF(-t * .32, -t * .02));
            shape.cubicTo(c + QPointF(-t * .32, -t * .46), c + QPointF(t * .32, -t * .46),
                          c + QPointF(t * .32, -t * .02));
            shape.lineTo(c + QPointF(t * .32, t * .3));
            shape.lineTo(c + QPointF(t * .15, t * .18));
            shape.lineTo(c + QPointF(0, t * .3));
            shape.lineTo(c + QPointF(-t * .15, t * .18));
            shape.closeSubpath();
            p.drawPath(shape);
        } else if (value == 2) {
            p.setBrush(bad ? QColor("#e46b78") : QColor("#dba0ab"));
            p.drawPolygon(QPolygonF{c + QPointF(-t * .4, t * .34), c + QPointF(0, -t * .2),
                                    c + QPointF(t * .4, t * .34)});
            p.drawEllipse(c + QPointF(0, -t * .1), t * .24, t * .25);
        } else {
            p.setBrush(bad ? QColor("#e46b78") : QColor("#88b08b"));
            p.drawRoundedRect(box.adjusted(t * .2, t * .17, -t * .2, -t * .17), t * .06, t * .06);
        }
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#263044"));
        p.drawEllipse(c + QPointF(-t * .11, -t * .06), t * .045, t * .065);
        p.drawEllipse(c + QPointF(t * .11, -t * .06), t * .045, t * .065);
        p.setPen(QPen(QColor("#263044"), std::max(.8, t * .035)));
        p.drawLine(c + QPointF(-t * .1, t * .12), c + QPointF(t * .1, t * .12));
        if (value == 2) {
            p.setBrush(Qt::white);
            p.setPen(Qt::NoPen);
            for (int sign : {-1, 1}) {
                auto a = c + QPointF(sign * t * .07, t * .1);
                p.drawPolygon(QPolygonF{a, a + QPointF(t * .05, 0), a + QPointF(t * .025, t * .1)});
            }
        }
    }
    void paintEvent(QPaintEvent *) override {
        if (!state)
            return;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const auto rect = boardRect();
        qreal t = rect.width() / columns;
        QColor ink = palette().color(QPalette::Text), error("#e46b78");
        const bool ascii = undead_ascii(input);
        for (int cell = 0; cell < config.width * config.height; cell++) {
            auto c = gridPoint(cell);
            QRectF box(c - QPointF(t / 2, t / 2), QSizeF(t, t));
            int mirror = undead_mirror(state, cell), value = undead_value(state, cell);
            bool bad = undead_cell_error(state, cell);
            p.fillRect(box, palette().color(QPalette::Base));
            if (undead_cursor(input, config.width) == cell) {
                QColor hi = palette().color(QPalette::Highlight);
                hi.setAlpha(90);
                if (undead_cursor_pencil(input)) {
                    p.setPen(Qt::NoPen);
                    p.setBrush(hi);
                    p.drawPolygon(QPolygonF{box.topLeft(), box.topLeft() + QPointF(t * .45, 0),
                                            box.topLeft() + QPointF(0, t * .45)});
                } else
                    p.fillRect(box, hi);
            }
            p.setPen(QPen(palette().color(QPalette::Mid), .8));
            p.setBrush(Qt::NoBrush);
            p.drawRect(box);
            if (mirror) {
                p.setPen(QPen(bad ? error : ink, std::max(2.0, t * .08), Qt::SolidLine, Qt::RoundCap));
                if (mirror == 1)
                    p.drawLine(box.topLeft() + QPointF(t * .2, t * .2),
                               box.bottomRight() - QPointF(t * .2, t * .2));
                else
                    p.drawLine(box.topRight() + QPointF(-t * .2, t * .2),
                               box.bottomLeft() + QPointF(t * .2, -t * .2));
            } else if (value == 1 || value == 2 || value == 4)
                drawMonster(p, box, value, ascii, bad);
            else {
                int marks = undead_pencils(state, cell);
                for (int m = 0; m < 3; m++)
                    if (marks & (1 << m)) {
                        QRectF small(box.left() + t * (m % 2) / 2, box.top() + t * (m / 2) / 2, t / 2, t / 2);
                        drawMonster(p, small, 1 << m, ascii, false);
                    }
            }
            if (bad) {
                p.setBrush(Qt::NoBrush);
                p.setPen(QPen(error, 1.2));
                p.drawRect(box.adjusted(1, 1, -1, -1));
            }
        }
        auto f = font();
        f.setPixelSize(std::max(1, int(t * .5)));
        p.setFont(f);
        for (int y = 0; y < rows; y++)
            for (int x = 0; x < columns; x++)
                if (((x == 0 || x == columns - 1) && y > 0 && y < rows - 1) ||
                    ((y == 0 || y == rows - 1) && x > 0 && x < columns - 1)) {
                    auto c = cellCentre(x + y * columns);
                    p.setPen(undead_clue_error(state, x, y)  ? error
                             : undead_clue_done(state, x, y) ? palette().color(QPalette::Mid)
                                                             : ink);
                    p.drawText(QRectF(c - QPointF(t / 2, t / 2), QSizeF(t, t)), Qt::AlignCenter,
                               QString::number(undead_clue(state, x, y)));
                }
    }

  private:
    const UndeadBackend *state{};
    const UndeadInput *input{};
    UndeadConfig config{};
};
UndeadGame::State UndeadGame::take(UndeadBackend *value) { return State(value, undead_free); }
UndeadGame::UndeadGame(Ui::note_widget *ui, QObject *parent) : Game(ui, parent) {
    view = new UndeadView(ui->undeadBoard);
    ui->undeadBoardLayout->addWidget(view);
    view->installEventFilter(this);
    for (int i = 0; i < undead_preset_count(); i++) {
        auto c = undead_preset(i);
        ui->undeadPreset->addItem(tr("%1×%2 · %3")
                                      .arg(c.width)
                                      .arg(c.height)
                                      .arg(QString::fromUtf8(undead_difficulty_name(c.difficulty))));
    }
    ui->undeadPreset->addItem(tr("自定义…"));
    for (int i = 0; i < 3; i++)
        ui->undeadDifficulty->addItem(QString::fromUtf8(undead_difficulty_name(i)));
    for (auto *combo : {ui->undeadPreset, ui->undeadDifficulty}) {
        combo->setMinimumContentsLength(5);
        combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        expandComboBox(combo);
    }
    for (auto *spin : {ui->undeadWidth, ui->undeadHeight}) {
        spin->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        spin->setMinimumWidth(48);
    }
    ui->undeadPreset->setCurrentIndex(1);
    connect(ui->undeadPreset, &QComboBox::currentIndexChanged, this, &UndeadGame::selectPreset);
    connect(ui->undeadApplyButton, &QToolButton::clicked, this, &UndeadGame::applyCustom);
    connect(ui->undeadNewButton, &QToolButton::clicked, this, &UndeadGame::reset);
    connect(ui->undeadRetryButton, &QToolButton::clicked, this, &UndeadGame::retry);
    connect(ui->undeadUndoButton, &QToolButton::clicked, this, &UndeadGame::undo);
    connect(ui->undeadRedoButton, &QToolButton::clicked, this, &UndeadGame::redo);
    connect(ui->undeadSolveButton, &QToolButton::clicked, this, [this] {
        if (!busy)
            adopt(take(undead_solve(initial.get(), state.get())));
    });

    for (auto *box : {ui->undeadKeepHighlight, ui->undeadLetters})
        connect(box, &QCheckBox::toggled, this, [this] { applyPreferences(); });
    connect(ui->undeadCountStyle, &QComboBox::currentIndexChanged, this, [this] { applyPreferences(); });
    for (int monster = 0; monster < 3; monster++) {
        auto *button = std::array<QToolButton *, 3>{ui->undeadGhostButton, ui->undeadVampireButton,
                                                    ui->undeadZombieButton}[monster];
        button->installEventFilter(this);
        connect(button, &QToolButton::clicked, this, [this, monster] {
            if (!busy && state)
                adopt(take(undead_monster_button(input.get(), state.get(), monster, false)));
            view->setFocus();
        });
    }
    connect(ui->undeadEraseButton, &QToolButton::clicked, this, [this] {
        send(PuzzleSpace);
        view->setFocus();
    });
    connect(ui->undeadMarksButton, &QToolButton::clicked, this, [this] {
        send('M');
        view->setFocus();
    });
    selectPreset();
}
void UndeadGame::fillCustom() {
    ui->undeadWidth->setValue(config.width);
    ui->undeadHeight->setValue(config.height);
    ui->undeadDifficulty->setCurrentIndex(config.difficulty);
}
void UndeadGame::selectPreset() {
    const int index = ui->undeadPreset->currentIndex();
    const bool custom = index == undead_preset_count();
    ui->undeadCustomSettings->setVisible(custom);
    if (!custom)
        config = undead_preset(index);
    fillCustom();
    if (!custom)
        reset();
}
void UndeadGame::applyCustom() {
    UndeadConfig next{ui->undeadWidth->value(), ui->undeadHeight->value(),
                      ui->undeadDifficulty->currentIndex()};
    if (const auto *error = undead_validate(next)) {
        ui->undeadFeedback->setText(tr("参数无效：%1").arg(QString::fromUtf8(error)));
        return;
    }
    config = next;
    reset();
}
void UndeadGame::reset() {
    const unsigned ticket = ++generation;
    busy = true;
    input.reset();
    state.reset();
    initial.reset();
    history.clear();
    future.clear();
    refresh();
    const auto parameters = config;
    const auto seed = QByteArray::number(QRandomGenerator::global()->generate64());
    auto *watcher = new QFutureWatcher<State>(this);
    connect(watcher, &QFutureWatcher<State>::finished, this, [this, watcher, ticket] {
        auto result = watcher->result();
        watcher->deleteLater();
        if (ticket != generation)
            return;
        busy = false;
        state = std::move(result);
        initial = take(undead_copy(state.get()));
        if (state)
            input.reset(undead_input_new(state.get()));
        applyPreferences();
    });
    watcher->setFuture(
        QtConcurrent::run([parameters, seed] { return take(undead_new(parameters, seed.constData())); }));
}
void UndeadGame::retry() {
    if (!busy && initial)
        adopt(take(undead_copy(initial.get())));
}
void UndeadGame::adopt(State next) {
    if (next) {
        history.push_back(state);
        future.clear();
        undead_input_changed(input.get(), state.get(), next.get());
        state = std::move(next);
    }
    refresh();
}
void UndeadGame::undo() {
    if (busy || history.empty())
        return;
    future.push_back(state);
    undead_input_changed(input.get(), state.get(), history.back().get());
    state = std::move(history.back());
    history.pop_back();
    refresh();
}
void UndeadGame::redo() {
    if (busy || future.empty())
        return;
    history.push_back(state);
    undead_input_changed(input.get(), state.get(), future.back().get());
    state = std::move(future.back());
    future.pop_back();
    refresh();
}
void UndeadGame::send(int button, int x, int y, bool control, bool shift) {
    if (!busy && state)
        adopt(take(undead_input(input.get(), state.get(), x, y, button, control, shift)));
}
void UndeadGame::applyPreferences() {
    undead_preferences(input.get(), ui->undeadKeepHighlight->isChecked(), ui->undeadLetters->isChecked(),
                       ui->undeadCountStyle->currentIndex());
    refresh();
}
void UndeadGame::refresh() {
    if (input) {
        QSignalBlocker letters(ui->undeadLetters), style(ui->undeadCountStyle);
        ui->undeadLetters->setChecked(undead_ascii(input.get()));
        ui->undeadCountStyle->setCurrentIndex(undead_count_style(input.get()));
    }
    view->setState(state.get(), input.get(), config);
    ui->undeadStatus->setText(tr("%1×%2 · %3")
                                  .arg(config.width)
                                  .arg(config.height)
                                  .arg(QString::fromUtf8(undead_difficulty_name(config.difficulty))));
    const std::array<QToolButton *, 3> buttons = {ui->undeadGhostButton, ui->undeadVampireButton,
                                                  ui->undeadZombieButton};
    const QStringList names = {tr("幽灵 G"), tr("吸血鬼 V"), tr("僵尸 Z")};
    for (int m = 0; m < 3; m++) {
        QString count;
        QColor color = ui->undeadBoard->palette().color(QPalette::Text);
        if (state) {
            int total = undead_total(state.get(), m), placed = undead_placed(state.get(), m),
                style = undead_count_style(input.get());
            count = style == 0   ? QString::number(total)
                    : style == 1 ? (placed < total ? QStringLiteral("−%1").arg(total - placed)
                                                   : QString::number(placed - total))
                                 : tr("%1／%2").arg(placed).arg(total);
            if (undead_count_error(state.get(), m))
                color = QColor("#e46b78");
            else if (placed == total)
                color = ui->undeadBoard->palette().color(QPalette::Mid);
        }
        // The application theme sets button text in QSS, so a palette alone
        // cannot expose the original count-error and completed-count colours.
        // Set the normal colour explicitly too: clearing a previous QSS colour
        // can leave the themed button palette with the old completed colour.
        buttons[m]->setStyleSheet(QStringLiteral("QToolButton:enabled { color: %1; }").arg(color.name()));
        buttons[m]->setText(names[m] + QStringLiteral(" · ") + count);
        buttons[m]->setEnabled(!busy && bool(state));
        buttons[m]->setToolTip(tr("点击填入；右键或 Control＋左键切换数量显示方式"));
    }
    ui->undeadFeedback->setText(busy                          ? tr("正在生成棋盘并校验可解性…")
                                : !state                      ? tr("未能生成棋盘，请开新一局。")
                                : undead_cheated(state.get()) ? tr("已显示答案，可撤销返回。")
                                : undead_won(state.get())
                                    ? tr("怪物与每条视线都吻合，完成啦！")
                                    : tr("红色提示数量或视线冲突；边缘数字可点击标记完成。"));
    ui->undeadRetryButton->setEnabled(!busy && bool(state));
    ui->undeadSolveButton->setEnabled(!busy && bool(state));
    ui->undeadUndoButton->setEnabled(!busy && !history.empty());
    ui->undeadRedoButton->setEnabled(!busy && !future.empty());
    ui->undeadEraseButton->setEnabled(!busy && bool(state));
    ui->undeadMarksButton->setEnabled(!busy && bool(state));
    QStringList cells, clues;
    if (state) {
        for (int c = 0; c < config.width * config.height; c++)
            cells << QStringLiteral("%1:%2:%3")
                         .arg(undead_mirror(state.get(), c))
                         .arg(undead_value(state.get(), c))
                         .arg(undead_pencils(state.get(), c));
        for (int y = 0; y < config.height + 2; y++)
            for (int x = 0; x < config.width + 2; x++)
                if (((x == 0 || x == config.width + 1) && y > 0 && y <= config.height) ||
                    ((y == 0 || y == config.height + 1) && x > 0 && x <= config.width))
                    clues << QString::number(undead_clue(state.get(), x, y));
    }
    view->setAccessibleDescription(
        ui->undeadStatus->text() +
        tr("；镜面、怪物、候选 %1；边缘线索 %2").arg(cells.join(','), clues.join(',')));
}
bool UndeadGame::eventFilter(QObject *watched, QEvent *event) {
    if (watched != view) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() == Qt::RightButton ||
                (mouse->button() == Qt::LeftButton && mouse->modifiers().testFlag(Qt::ControlModifier))) {
                send('C');
                return true;
            }
        }
        return Game::eventFilter(watched, event);
    }
    if (event->type() == QEvent::PaletteChange) {
        refresh();
        return false;
    }
    if (event->type() == QEvent::MouseButtonPress) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() != Qt::LeftButton && mouse->button() != Qt::RightButton)
            return false;
        view->setFocus(Qt::MouseFocusReason);
        const auto p = view->inputPoint(mouse->position());
        send(mouse->button() == Qt::RightButton || mouse->modifiers().testFlag(Qt::ControlModifier)
                 ? PuzzleRight
                 : PuzzleLeft,
             p.x(), p.y());
        return true;
    }
    if (event->type() == QEvent::KeyPress) {
        auto *key = static_cast<QKeyEvent *>(event);
        if (key->matches(QKeySequence::Undo)) {
            undo();
            return true;
        }
        if (key->matches(QKeySequence::Redo)) {
            redo();
            return true;
        }
        int button = 0;
        switch (key->key()) {
        case Qt::Key_Up:
            button = PuzzleUp;
            break;
        case Qt::Key_Down:
            button = PuzzleDown;
            break;
        case Qt::Key_Left:
            button = PuzzleArrowLeft;
            break;
        case Qt::Key_Right:
            button = PuzzleArrowRight;
            break;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            button = PuzzleEnter;
            break;
        case Qt::Key_Space:
            button = PuzzleSpace;
            break;
        case Qt::Key_U:
            undo();
            return true;
        case Qt::Key_R:
            redo();
            return true;
        case Qt::Key_Backspace:
        case Qt::Key_Delete:
            button = '\b';
            break;
        case Qt::Key_G:
            button = 'G';
            break;
        case Qt::Key_V:
            button = 'V';
            break;
        case Qt::Key_Z:
            button = 'Z';
            break;
        case Qt::Key_E:
            button = 'E';
            break;
        case Qt::Key_M:
            button = 'M';
            break;
        case Qt::Key_A:
            button = 'A';
            break;
        case Qt::Key_C:
            button = 'C';
            break;
        case Qt::Key_0:
            button = '0';
            break;
        case Qt::Key_1:
            button = '1';
            break;
        case Qt::Key_2:
            button = '2';
            break;
        case Qt::Key_3:
            button = '3';
            break;
        default:
            return Game::eventFilter(watched, event);
        }
        send(button, -100, -100, key->modifiers().testFlag(Qt::ControlModifier),
             key->modifiers().testFlag(Qt::ShiftModifier));
        return true;
    }
    return Game::eventFilter(watched, event);
}
