#ifndef CHARTNAVIGATION_GAMES_ARITHMETICEXPRESSION_HPP
#define CHARTNAVIGATION_GAMES_ARITHMETICEXPRESSION_HPP

#include <QString>
#include <cmath>
#include <optional>
#include <vector>

namespace GameArithmetic {
inline constexpr double twentyFourTolerance = 1e-8;

// Parse only card numbers, the four arithmetic operators and parentheses.
// Record the leaves so callers can enforce the available numbers and multiplicities.
struct ArithmeticExpression {
    QString text;
    qsizetype position{};
    std::vector<int> cards;
    size_t maximumCards{4};
    int maximumNumber{13};
    bool positiveIntegersOnly{};

    bool validIntermediate (double value) const {
        return std::isfinite(value) && (!positiveIntegersOnly || (value > 0 && value == std::floor(value)));
    }

    void skipSpace () {
        while (position < text.size() && text[position].isSpace())
            ++position;
    }

    bool take (const QChar token) {
        skipSpace();
        if (position < text.size() && text[position] == token) {
            ++position;
            return true;
        }
        return false;
    }

    std::optional<double> factor () {
        if (take('(')) {
            const auto value = expression();
            if (!value || !take(')'))
                return std::nullopt;
            return value;
        }
        const auto start = position;
        while (position < text.size() && text[position] >= '0' && text[position] <= '9')
            ++position;
        if (position == start || cards.size() == maximumCards)
            return std::nullopt;
        bool valid{};
        const int card = text.mid(start, position - start).toInt(&valid);
        if (!valid || card < 1 || card > maximumNumber)
            return std::nullopt;
        cards.push_back(card);
        return card;
    }

    std::optional<double> term () {
        auto value = factor();
        while (value) {
            const bool multiply = take('*');
            if (!multiply && !take('/'))
                break;
            const auto right = factor();
            if (!right || (!multiply && std::abs(*right) < twentyFourTolerance))
                return std::nullopt;
            *value = multiply ? *value * *right : *value / *right;
            if (!validIntermediate(*value))
                return std::nullopt;
        }
        return value;
    }

    std::optional<double> expression () {
        auto value = term();
        while (value) {
            const bool add = take('+');
            if (!add && !take('-'))
                break;
            const auto right = term();
            if (!right)
                return std::nullopt;
            *value = add ? *value + *right : *value - *right;
            if (!validIntermediate(*value))
                return std::nullopt;
        }
        return value;
    }
};
}

#endif // CHARTNAVIGATION_GAMES_ARITHMETICEXPRESSION_HPP
