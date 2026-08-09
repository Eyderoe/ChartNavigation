#include "replay_control.hpp"
#include "ui_replay_control.h"

#include <QPushButton>
#include <QSlider>


replay_control::replay_control (QWidget *parent) :
    QWidget(parent), ui(new Ui::replay_control) {
    ui->setupUi(this);
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
}

void replay_control::setDuration (const qint64 durationMs) {
    duration = durationMs;
}

void replay_control::setPosition (const qint64 timeMs) {
    updating = true;
    if (duration <= 0)
        ui->horizontalSlider->setValue(0);
    else
        ui->horizontalSlider->setValue(static_cast<int>(timeMs * 999 / duration));
    updating = false;
}
