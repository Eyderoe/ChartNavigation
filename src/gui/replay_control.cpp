#include "replay_control.hpp"
#include "ui_replay_control.h"

#include <QClipboard>
#include <QEvent>
#include <QGuiApplication>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QStringList>

namespace
{
/**
 * @brief 把毫秒格式化为 HH:MM:SS.mmm
 */
QString formatTime (const qint64 timeMs) {
    const qint64 totalSecs = timeMs / 1000;
    const int hours = static_cast<int>(totalSecs / 3600);
    const int minutes = static_cast<int>((totalSecs % 3600) / 60);
    const int seconds = static_cast<int>(totalSecs % 60);
    const int millis = static_cast<int>(timeMs % 1000);
    return QString("%1:%2:%3.%4")
           .arg(hours, 2, 10, QChar('0'))
           .arg(minutes, 2, 10, QChar('0'))
           .arg(seconds, 2, 10, QChar('0'))
           .arg(millis, 3, 10, QChar('0'));
}

/**
 * @brief 解析跳转输入: 纯毫秒, 或 HH:MM:SS(.mmm) / MM:SS(.mmm) / SS(.mmm)
 * @return 毫秒时间, 输入非法时返回 -1
 */
qint64 parseTime (const QString &text) {
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return -1;

    bool ok = false;
    const qint64 rawMs = trimmed.toLongLong(&ok);
    if (ok)
        return rawMs;

    const QStringList parts = trimmed.split(':');
    if (parts.isEmpty() || parts.size() > 3)
        return -1;

    qint64 ms = 0;
    for (int i = 0; i < parts.size(); ++i) {
        const bool isLast = (i == parts.size() - 1);
        if (isLast) {
            // 最后一段允许带毫秒小数: SS(.mmm)
            const QStringList frac = parts[i].split('.');
            if (frac.size() > 2)
                return -1;
            bool okSec = false;
            const qint64 sec = frac[0].toLongLong(&okSec);
            if (!okSec || sec < 0)
                return -1;
            ms += sec * 1000;
            if (frac.size() == 2) {
                bool okMilli = false;
                const int milli = frac[1].leftJustified(3, '0').left(3).toInt(&okMilli);
                if (!okMilli)
                    return -1;
                ms += milli;
            }
        } else {
            bool okSeg = false;
            const qint64 value = parts[i].toLongLong(&okSeg);
            if (!okSeg || value < 0)
                return -1;
            ms += value * (i == 0 && parts.size() == 3 ? 3600 : 60) * 1000;
        }
    }
    return ms;
}
} // namespace


replay_control::replay_control (QWidget *parent) :
    QWidget(parent), ui(new Ui::replay_control) {
    ui->setupUi(this);
    ui->lineEdit->setReadOnly(true);
    ui->lineEdit->setPlaceholderText("00:00:00.000");
    ui->lineEdit_2->setPlaceholderText("HH:MM:SS 或毫秒");
    ui->lineEdit->installEventFilter(this);
    initConnect();
}

replay_control::~replay_control () {
    delete ui;
}

void replay_control::initConnect () {
    connect(ui->horizontalSlider, &QSlider::valueChanged, this, [this](const int value) {
        if (!updating)
            emit seekPercentRequested(value * 100 / 999);
    });
    connect(ui->pushButton_2, &QPushButton::clicked, this, [this] { emit stepEventsRequested(-10); });
    connect(ui->pushButton, &QPushButton::clicked, this, [this] { emit stepEventsRequested(10); });
    connect(ui->pushButton_4, &QPushButton::clicked, this, [this] { emit stepPercentRequested(-1); });
    connect(ui->pushButton_3, &QPushButton::clicked, this, [this] { emit stepPercentRequested(1); });
    connect(ui->pushButton_5, &QPushButton::clicked, this, &replay_control::jumpToInputTime);
    connect(ui->lineEdit_2, &QLineEdit::returnPressed, this, &replay_control::jumpToInputTime);
    connect(ui->pushButton_6, &QPushButton::clicked, this, [this] { emit pauseRequested(); });
    connect(ui->pushButton_7, &QPushButton::clicked, this, [this] { emit resumeRequested(); });
}

void replay_control::setDuration (const qint64 durationMs) {
    duration = durationMs;
    ui->lineEdit->setText(formatTime(0));
}

void replay_control::setPosition (const qint64 timeMs) {
    updating = true;
    if (duration <= 0)
        ui->horizontalSlider->setValue(0);
    else
        ui->horizontalSlider->setValue(static_cast<int>(timeMs * 999 / duration));
    updating = false;
    ui->lineEdit->setText(formatTime(timeMs));
}

void replay_control::jumpToInputTime () {
    const qint64 timeMs = parseTime(ui->lineEdit_2->text());
    if (timeMs < 0)
        return; // 输入不合法, 忽略
    emit seekTimeRequested(timeMs);
    ui->lineEdit_2->selectAll();
}

bool replay_control::eventFilter (QObject *watched, QEvent *event) {
    // 点击"当前时间"框时, 把时间复制到剪贴板, 方便直接粘贴到跳转框
    if (watched == ui->lineEdit && event->type() == QEvent::MouseButtonPress) {
        QGuiApplication::clipboard()->setText(ui->lineEdit->text());
        return false; // 不拦截, 保持正常的焦点/选中行为
    }
    return QWidget::eventFilter(watched, event);
}
