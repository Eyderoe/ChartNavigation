#ifndef CHARTNAVIGATION_ANDROID_HPP
#define CHARTNAVIGATION_ANDROID_HPP

#include <QDebug>


template <typename... Args>
QString debugAllToString (Args &&... args);

void grantAllFilesPermission ();
void persistAndroidTreeUri (const QString &uri);
void copyAndroidDebugText ();
void grantFolderPermission ();
bool hasManageExternalStorage ();


inline QString androidDebugText;
inline QMutex androidDebugMutex;


/**
 * @brief 是否有"所有文件访问权限"(惰性获取)
 * @note 不能在全局初始化时调用 hasManageExternalStorage(), 那时 JNI 环境尚未就绪;
 *       静态局部变量保证只在第一次使用时才执行, 且线程安全
 */
inline bool androidMasterAccess () {
    static const bool access = hasManageExternalStorage();
    return access;
}


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

#endif //CHARTNAVIGATION_ANDROID_HPP
