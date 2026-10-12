#include "pyramid.hpp"
#include "ui_note_widget.h"
#include <QIntValidator>
#include <QRandomGenerator>
#include <algorithm>


PyramidGame::PyramidGame (Ui::note_widget *ui, QObject *parent) : Game(ui, parent) {
    initPyramid();
}

void PyramidGame::initPyramid () {
    ui->pyramidRules->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    ui->pyramidFeedback->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    for (int row = 0; row < 5; ++row) {
        for (int column = 0; column <= row; ++column) {
            const int index = row * (row + 1) / 2 + column;
            auto *cell = new QLineEdit(ui->pagePyramid);
            cell->setObjectName(QStringLiteral("pyramidCell%1").arg(index));
            cell->setAlignment(Qt::AlignCenter);
            cell->setValidator(new QIntValidator(1, 999, cell));
            cell->setMaxLength(3);
            cell->setMinimumWidth(0);
            cell->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
            cell->setAccessibleName(tr("第%1层，第%2格").arg(row + 1).arg(column + 1));
            ui->pyramidGrid->addWidget(cell, row, 4 - row + 2 * column, 1, 2);
            pyramidCells[index] = cell;
            connect(cell, &QLineEdit::returnPressed, this, &PyramidGame::checkPyramid);
            connect(cell, &QLineEdit::textChanged, this, [this] { ui->pyramidFeedback->clear(); });
        }
    }
    for (int column = 0; column < 10; ++column)
        ui->pyramidGrid->setColumnStretch(column, 1);
    connect(ui->pyramidNewButton, &QToolButton::clicked, this, &PyramidGame::resetPyramid);
    connect(ui->pyramidCheckButton, &QToolButton::clicked, this, &PyramidGame::checkPyramid);
    connect(ui->pyramidAnswerButton, &QToolButton::clicked, this, [this] {
        for (size_t i = 0; i < pyramidCells.size(); ++i)
            pyramidCells[i]->setText(QString::number(pyramidNumbers[i]));
        ui->pyramidFeedback->setText(tr("已填入参考答案，点击换题再挑战一次。"));
    });
    resetPyramid();
}

void PyramidGame::resetPyramid () {
    for (int i = 10; i < 15; ++i)
        pyramidNumbers[i] = QRandomGenerator::global()->bounded(1, 20);
    for (int row = 3; row >= 0; --row)
        for (int column = 0; column <= row; ++column) {
            const int below = (row + 1) * (row + 2) / 2 + column;
            pyramidNumbers[row * (row + 1) / 2 + column] = pyramidNumbers[below] + pyramidNumbers[below + 1];
        }
    // Remove clues in random order, keeping only removals that still allow the
    // whole pyramid to be deduced by local addition/subtraction. Each deduction
    // determines exactly one value, so the remaining clues give a unique solution.
    const auto canComplete = [](std::array<bool, 15> known) {
        bool changed;
        do {
            changed = false;
            for (int row = 0; row < 4; ++row)
                for (int column = 0; column <= row; ++column) {
                    const int above = row * (row + 1) / 2 + column;
                    const int below = (row + 1) * (row + 2) / 2 + column;
                    if (static_cast<int>(known[above]) + known[below] + known[below + 1] == 2) {
                        known[above] = known[below] = known[below + 1] = true;
                        changed = true;
                    }
                }
        } while (changed);
        return std::ranges::all_of(known, [](bool value) { return value; });
    };
    std::array<bool, 15> clues;
    clues.fill(true);
    std::array<int, 15> removalOrder{};
    for (int i = 0; i < 15; ++i)
        removalOrder[i] = i;
    for (int i = 0; i < 15; ++i) {
        std::swap(removalOrder[i], removalOrder[QRandomGenerator::global()->bounded(i, 15)]);
        const int index = removalOrder[i];
        clues[index] = false;
        if (!canComplete(clues))
            clues[index] = true;
    }
    for (int row = 0; row < 5; ++row)
        for (int column = 0; column <= row; ++column) {
            const int index = row * (row + 1) / 2 + column;
            auto *cell = pyramidCells[index];
            const bool clue = clues[index];
            cell->setReadOnly(clue);
            cell->setFocusPolicy(clue ? Qt::NoFocus : Qt::StrongFocus);
            auto font = cell->font();
            font.setBold(clue);
            cell->setFont(font);
            cell->setText(clue ? QString::number(pyramidNumbers[index]) : QString{});
            cell->setPlaceholderText(clue ? QString{} : QStringLiteral("?"));
        }
    ui->pyramidFeedback->clear();
}

void PyramidGame::checkPyramid () const {
    int missing = 0;
    int incorrect = 0;
    for (size_t i = 0; i < pyramidCells.size(); ++i) {
        if (pyramidCells[i]->text().isEmpty())
            ++missing;
        else if (pyramidCells[i]->text().toInt() != pyramidNumbers[i])
            ++incorrect;
    }
    if (missing)
        ui->pyramidFeedback->setText(tr("还有 %1 格没有填写。").arg(missing));
    else if (incorrect)
        ui->pyramidFeedback->setText(tr("有 %1 格不正确，再检查一下相邻两格的和。").arg(incorrect));
    else
        ui->pyramidFeedback->setText(tr("全部正确！点击换题继续。"));
}

