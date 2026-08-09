#ifndef CHARTNAVIGATION_ANDROIDDEBUG_HPP
#define CHARTNAVIGATION_ANDROIDDEBUG_HPP


#include <QDebug>
#include <QString>


inline QString androidDebugText;
inline QMutex androidDebugMutex;

template <typename... Args>
QString debugAllToString (Args &&... args) {
    QMutexLocker locker(&androidDebugMutex);
    {
        QDebug dbg(&androidDebugText);
        dbg.nospace().noquote();
        (dbg << ... << std::forward<Args>(args));
    }
    return androidDebugText;
}


#endif //CHARTNAVIGATION_ANDROIDDEBUG_HPP
