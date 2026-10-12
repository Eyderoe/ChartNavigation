#include "guessNumber.hpp"
#include "ui_note_widget.h"
#include <QRandomGenerator>
#include <QRegularExpression>
#include <algorithm>
#include <array>


GuessNumberGame::GuessNumberGame (Ui::note_widget *ui, QObject *parent) : Game(ui, parent) {
    initGuessNumber();
}

void GuessNumberGame::initGuessNumber () {
    ui->guessNumberRules->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    connect(ui->guessNumberNewButton, &QToolButton::clicked, this, &GuessNumberGame::resetGuessNumber);
    connect(ui->guessNumberInput, &QLineEdit::returnPressed, this, &GuessNumberGame::checkGuessNumber);
    connect(ui->guessNumberInput, &QLineEdit::textChanged, this, [this] {
        if (!guessNumberFinished)
            ui->guessNumberFeedback->clear();
    });
    resetGuessNumber();
}

void GuessNumberGame::resetGuessNumber () {
    std::array<int, 10> digits{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    guessNumberAnswer.clear();
    for (int i = 0; i < 4; ++i) {
        const int next = QRandomGenerator::global()->bounded(i, static_cast<int>(digits.size()));
        std::swap(digits[i], digits[next]);
        guessNumberAnswer += QChar('0' + digits[i]);
    }
    guessNumberAttempts = 0;
    guessNumberFinished = false;
    ui->guessNumberInput->setReadOnly(false);
    ui->guessNumberInput->clear();
    ui->guessNumberFeedback->clear();
    ui->guessNumberHistory->clear();
}

void GuessNumberGame::checkGuessNumber () {
    if (guessNumberFinished)
        return;
    const auto guess = ui->guessNumberInput->text().trimmed();
    static const QRegularExpression fourDigits{QStringLiteral("^[0-9]{4}$")};
    if (!fourDigits.match(guess).hasMatch()) {
        ui->guessNumberFeedback->setText(tr("请输入四位数字（0–9，可以以0开头）。"));
        return;
    }
    for (qsizetype i = 0; i < guess.size(); ++i) {
        if (guess.indexOf(guess[i]) != i) {
            ui->guessNumberFeedback->setText(tr("四个数字不能重复。"));
            return;
        }
    }
    int exact = 0;
    int misplaced = 0;
    for (qsizetype i = 0; i < guess.size(); ++i) {
        if (guess[i] == guessNumberAnswer[i])
            ++exact;
        else if (guessNumberAnswer.contains(guess[i]))
            ++misplaced;
    }
    ++guessNumberAttempts;
    ui->guessNumberHistory->appendPlainText(tr("%1.  %2  →  %3A%4B")
        .arg(guessNumberAttempts).arg(guess).arg(exact).arg(misplaced));
    guessNumberFinished = exact == 4;
    ui->guessNumberFeedback->setText(guessNumberFinished
        ? tr("猜对了！共猜了%1次，点击新一局继续。").arg(guessNumberAttempts)
        : tr("%1A%2B：%1个位置正确，%2个数字正确但位置不对。").arg(exact).arg(misplaced));
    if (guessNumberFinished)
        ui->guessNumberInput->setReadOnly(true);
    else
        ui->guessNumberInput->selectAll();
}

