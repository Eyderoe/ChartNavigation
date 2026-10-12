#include "twentyFour.hpp"
#include "ui_note_widget.h"
#include "arithmeticExpression.hpp"
#include <QFontMetrics>
#include <QRandomGenerator>
#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

using GameArithmetic::ArithmeticExpression;
using GameArithmetic::twentyFourTolerance;

namespace {

struct TwentyFourTerm {
    double value;
    QString expression;
};

// Combine every pair in both orders, including expressions with fractional intermediate values.
std::optional<QString> solveTwentyFour (const std::vector<TwentyFourTerm> &terms) {
    if (terms.size() == 1) {
        if (std::abs(terms.front().value - 24.0) < twentyFourTolerance)
            return terms.front().expression;
        return std::nullopt;
    }
    for (size_t i = 0; i < terms.size(); ++i) {
        for (size_t j = i + 1; j < terms.size(); ++j) {
            const auto &a = terms[i];
            const auto &b = terms[j];
            std::vector<TwentyFourTerm> remaining;
            for (size_t k = 0; k < terms.size(); ++k)
                if (k != i && k != j)
                    remaining.push_back(terms[k]);
            const auto combine = [&](const TwentyFourTerm &left, const TwentyFourTerm &right,
                                     const QChar operation, const double value) -> std::optional<QString> {
                remaining.push_back({value, "(" + left.expression + operation + right.expression + ")"});
                auto answer = solveTwentyFour(remaining);
                remaining.pop_back();
                return answer;
            };
            if (auto answer = combine(a, b, '+', a.value + b.value)) return answer;
            if (auto answer = combine(a, b, '*', a.value * b.value)) return answer;
            if (auto answer = combine(a, b, '-', a.value - b.value)) return answer;
            if (auto answer = combine(b, a, '-', b.value - a.value)) return answer;
            if (std::abs(b.value) > twentyFourTolerance)
                if (auto answer = combine(a, b, '/', a.value / b.value)) return answer;
            if (std::abs(a.value) > twentyFourTolerance)
                if (auto answer = combine(b, a, '/', b.value / a.value)) return answer;
        }
    }
    return std::nullopt;
}
}

TwentyFourGame::TwentyFourGame (Ui::note_widget *ui, QObject *parent) : Game(ui, parent) {
    initTwentyFour();
}

void TwentyFourGame::initTwentyFour () {
    auto font = ui->twentyFourCard1->font();
    font.setPointSizeF(font.pointSizeF() * 1.5);
    const QFontMetrics metrics(font);
    for (auto *card : {ui->twentyFourCard1, ui->twentyFourCard2, ui->twentyFourCard3, ui->twentyFourCard4}) {
        card->setAlignment(Qt::AlignCenter);
        card->setTextFormat(Qt::RichText);
        card->setStyleSheet("QLabel { border: 2px solid #71808a; border-radius: 6px; }");
        card->setFont(font);
        card->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        card->setFixedHeight(std::max(64, metrics.height() * 2 + 12));
    }
    for (int i = 0; i < 4; ++i)
        ui->twentyFourCardsLayout->setStretch(i, 1);
    connect(ui->twentyFourDealButton, &QToolButton::clicked, this, &TwentyFourGame::dealTwentyFour);
    connect(ui->twentyFourInput, &QLineEdit::returnPressed, this, &TwentyFourGame::checkTwentyFour);
    connect(ui->twentyFourInput, &QLineEdit::textChanged, this, [this](const QString &text) {
        ui->twentyFourFeedback->clear();
        if (text.trimmed().compare("inop", Qt::CaseInsensitive) == 0)
            ui->twentyFourInput->setText(twentyFourAnswer);
    });
    dealTwentyFour();
}

void TwentyFourGame::dealTwentyFour () {
    std::array<int, 4> deckCards{};
    std::optional<QString> answer;
    do {
        std::vector<TwentyFourTerm> terms;
        for (size_t i = 0; i < deckCards.size(); ++i) {
            int card;
            do {
                card = QRandomGenerator::global()->bounded(52);
            } while (std::find(deckCards.begin(), deckCards.begin() + i, card) != deckCards.begin() + i);
            deckCards[i] = card;
            twentyFourCards[i] = card % 13 + 1;
            terms.push_back({static_cast<double>(twentyFourCards[i]), QString::number(twentyFourCards[i])});
        }
        answer = solveTwentyFour(terms);
    } while (!answer);
    twentyFourAnswer = *answer;
    const std::array labels{ui->twentyFourCard1, ui->twentyFourCard2, ui->twentyFourCard3, ui->twentyFourCard4};
    const std::array<QString, 4> suits{"♠", "♥", "♣", "♦"};
    for (size_t i = 0; i < labels.size(); ++i) {
        const int suit = deckCards[i] / 13;
        const auto number = QString::number(twentyFourCards[i]);
        labels[i]->setText(suit == 1 || suit == 3
            ? "<span style='color:#d64a4a'>" + suits[suit] + "<br/>" + number + "</span>"
            : suits[suit] + "<br/>" + number);
    }
    ui->twentyFourInput->clear();
    ui->twentyFourFeedback->clear();
}

void TwentyFourGame::checkTwentyFour () const {
    auto text = ui->twentyFourInput->text();
    text.replace(QChar(0x00d7), '*').replace(QChar(0x00f7), '/');
    ArithmeticExpression parser{text};
    const auto result = parser.expression();
    parser.skipSpace();
    QString message;
    if (!result || parser.position != text.size() || !std::isfinite(*result)) {
        message = tr("请输入有效算式，使用 + - * / 和括号，不能除以0。");
    } else {
        std::ranges::sort(parser.cards);
        auto expected = twentyFourCards;
        std::ranges::sort(expected);
        if (!std::ranges::equal(parser.cards, expected))
            message = tr("需要把这四张牌各用一次。");
        else if (std::abs(*result - 24.0) >= twentyFourTolerance)
            message = tr("当前结果是 %1，还没有凑到24。").arg(*result, 0, 'g', 10);
        else
            message = tr("答对了！点击换牌继续。");
    }
    ui->twentyFourFeedback->setText(message);
}

