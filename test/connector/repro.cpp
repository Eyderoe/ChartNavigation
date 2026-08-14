#include <QCoreApplication>
#include <QDebug>
#include <QObject>
#include <QSettings>
#include <QTimer>
#include <cstdio>
#include <thread>

class SignalEmitter : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    void fire () { emit changed(42); }
signals:
    void changed (int);
};

class Receiver : public QObject {
    Q_OBJECT
public slots:
    void onChanged (int v) { qDebug() << "receiver got" << v; }
};

int main (int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QSettings::setDefaultFormat(QSettings::IniFormat);

    auto *emitter = new SignalEmitter;
    auto *receiver = new Receiver;
    QObject::connect(emitter, &SignalEmitter::changed, receiver, &Receiver::onChanged);

    std::thread worker([emitter] {
        qDebug() << "worker: Qt from foreign thread";
        QSettings settings;
        settings.setValue("probe", 1);
        settings.sync();
        emitter->fire();
    });

    QTimer::singleShot(150, &app, &QCoreApplication::quit);
    worker.join();
    qDebug() << "worker joined";

    const int rc = app.exec();
    delete emitter;
    delete receiver;
    std::printf("clean exit rc=%d\n", rc);
    return rc;
}

#include "moc_repro.cpp"
