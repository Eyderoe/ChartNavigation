#include "countdown.hpp"
#include "ui_note_widget.h"
#include "arithmeticExpression.hpp"
#include <QFutureWatcher>
#include <QRandomGenerator>
#include <QtConcurrent/QtConcurrentRun>
#include <algorithm>
#include <cmath>
#include <unordered_map>

using GameArithmetic::ArithmeticExpression;

namespace {
struct CountdownSolution {
    qint64 value;
    QString expression;
};

CountdownSolution solveCountdown (const std::array<int, 6> &numbers, int target) {
    // Each mask identifies physical number tiles, including duplicate values.
    // Retaining one expression per reachable value is sufficient for all future
    // combinations. Consider every nonempty subset, since unused tiles are legal.
    std::array<std::unordered_map<qint64, QString>, 64> reachable;
    CountdownSolution best{numbers[0], QString::number(numbers[0])};
    for (unsigned mask = 1; mask < reachable.size(); ++mask) {
        const auto add = [&](qint64 value, const QString &expression) {
            if (value <= 0)
                return;
            reachable[mask].try_emplace(value, expression);
            if (std::abs(value - target) < std::abs(best.value - target))
                best = {value, expression};
        };
        for (unsigned i = 0; i < numbers.size(); ++i)
            if (mask == (1u << i))
                add(numbers[i], QString::number(numbers[i]));
        for (unsigned left = (mask - 1) & mask; left; left = (left - 1) & mask) {
            const unsigned right = mask ^ left;
            if (left >= right)
                continue;
            for (const auto &[a, aExpression] : reachable[left])
                for (const auto &[b, bExpression] : reachable[right]) {
                    const auto combine = [&](qint64 value, const QString &first, QChar op, const QString &second) {
                        if (value > 0)
                            add(value, "(" + first + op + second + ")");
                    };
                    combine(a + b, aExpression, '+', bExpression);
                    combine(a * b, aExpression, '*', bExpression);
                    combine(a - b, aExpression, '-', bExpression);
                    combine(b - a, bExpression, '-', aExpression);
                    if (a % b == 0) combine(a / b, aExpression, '/', bExpression);
                    if (b % a == 0) combine(b / a, bExpression, '/', aExpression);
                    if (best.value == target)
                        return best;
                }
        }
        if (best.value == target)
            return best;
    }
    return best;
}
}

CountdownGame::CountdownGame (Ui::note_widget *ui, QObject *parent) : Game(ui, parent) {
    initCountdown();
}

void CountdownGame::initCountdown () {
    for (auto *label : {ui->countdownRules, ui->countdownNumbers, ui->countdownTarget, ui->countdownFeedback})
        label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    connect(ui->countdownNewButton, &QToolButton::clicked, this, &CountdownGame::resetCountdown);
    connect(ui->countdownCheckButton, &QToolButton::clicked, this, &CountdownGame::checkCountdown);
    connect(ui->countdownInput, &QLineEdit::returnPressed, this, &CountdownGame::checkCountdown);
    connect(ui->countdownInput, &QLineEdit::textChanged, this, [this] { ui->countdownFeedback->clear(); });
    connect(ui->countdownAnswerButton, &QToolButton::clicked, this, [this] {
        ui->countdownInput->setText(countdownAnswer);
        ui->countdownFeedback->setText(tr("最优解：%1 = %2，与目标相差 %3。")
            .arg(countdownAnswer).arg(countdownBestValue).arg(std::abs(countdownBestValue - countdownTarget)));
    });
    resetCountdown();
}

void CountdownGame::resetCountdown () {
    auto *random = QRandomGenerator::global();
    std::array<int, 4> large{25, 50, 75, 100};
    std::array<int, 20> small{};
    for (int i = 0; i < 20; ++i)
        small[i] = i / 2 + 1;
    for (int i = 0; i < 2; ++i) {
        std::swap(large[i], large[random->bounded(i, 4)]);
        countdownNumbers[i] = large[i];
    }
    for (int i = 0; i < 4; ++i) {
        std::swap(small[i], small[random->bounded(i, 20)]);
        countdownNumbers[i + 2] = small[i];
    }
    countdownTarget = random->bounded(100, 1000);
    countdownAnswer.clear();
    ui->countdownAnswerButton->setEnabled(false);
    ui->countdownNewButton->setEnabled(false);
    ui->countdownAnswerButton->setText(tr("计算最优解…"));
    // An unreachable target requires the full search. Keep that work off the UI
    // thread; the watcher disconnects automatically if this widget is destroyed.
    auto *watcher = new QFutureWatcher<CountdownSolution>(this);
    connect(watcher, &QFutureWatcher<CountdownSolution>::finished, this, [this, watcher] {
        const auto solution = watcher->result();
        countdownAnswer = solution.expression;
        countdownBestValue = solution.value;
        ui->countdownAnswerButton->setText(tr("查看最优解"));
        ui->countdownAnswerButton->setEnabled(true);
        ui->countdownNewButton->setEnabled(true);
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([numbers = countdownNumbers, target = countdownTarget] {
        return solveCountdown(numbers, target);
    }));
    QStringList numbers;
    for (const int number : countdownNumbers)
        numbers.append(QString::number(number));
    ui->countdownNumbers->setText(tr("可用数字：%1").arg(numbers.join(QStringLiteral("   "))));
    ui->countdownTarget->setText(tr("目标：%1").arg(countdownTarget));
    ui->countdownInput->clear();
    ui->countdownFeedback->clear();
}

void CountdownGame::checkCountdown () const {
    auto text = ui->countdownInput->text();
    text.replace(QChar(0x00d7), '*').replace(QChar(0x00f7), '/');
    ArithmeticExpression parser{text};
    parser.maximumCards = 6;
    parser.maximumNumber = 100;
    parser.positiveIntegersOnly = true;
    const auto result = parser.expression();
    parser.skipSpace();
    if (!result || parser.position != text.size()) {
        ui->countdownFeedback->setText(tr("请输入有效算式；只能使用四则运算和括号，所有中间结果必须是正整数。"));
        return;
    }
    auto available = countdownNumbers;
    std::ranges::sort(available);
    std::ranges::sort(parser.cards);
    if (!std::ranges::includes(available, parser.cards)) {
        ui->countdownFeedback->setText(tr("只能使用给定数字，每个数字最多使用一次（重复数字按给出的个数使用）。"));
        return;
    }
    const double distance = std::abs(*result - countdownTarget);
    QString message = tr("当前结果：%1，与目标相差 %2。").arg(*result, 0, 'g', 12).arg(distance, 0, 'g', 12);
    if (distance == 0)
        message += tr("精确命中，已达到最优！");
    else if (!countdownAnswer.isEmpty() && distance == std::abs(countdownBestValue - countdownTarget))
        message += tr("已达到最接近的结果！本题无法精确命中。");
    else
        message += tr("可以继续尝试，差值越小越好。");
    ui->countdownFeedback->setText(message);
}

