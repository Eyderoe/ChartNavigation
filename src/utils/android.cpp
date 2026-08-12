#include "android.hpp"

#if defined(__ANDROID__)
#include <QClipboard>
#include <QCoreApplication>
#include <QApplication>
#include <QJniObject>
#endif

/**
 * @brief 跳转系统设置, 授权"所有文件访问"(MANAGE_EXTERNAL_STORAGE, 仅安卓)
 * @note 安卓 11 (API 30) 以上才有此设置页; 授权后裸路径(/storage/emulated/0/...)才可读
 */
void grantAllFilesPermission () {
#if defined(__ANDROID__)
    // 安卓 11 (API 30) 以下没有该权限
    if (QJniObject::getStaticField<jint>("android/os/Build$VERSION", "SDK_INT") < 30)
        return;
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    // Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION)
    QJniObject intent("android/content/Intent", "(Ljava/lang/String;)V",
                      QJniObject::fromString("android.settings.MANAGE_APP_ALL_FILES_ACCESS_PERMISSION").object());
    // intent.setData(Uri.parse("package:" + getPackageName()))
    QJniObject packageName = context.callObjectMethod("getPackageName", "()Ljava/lang/String;");
    QJniObject uri = QJniObject::callStaticObjectMethod(
        "android/net/Uri", "parse", "(Ljava/lang/String;)Landroid/net/Uri;",
        QJniObject::fromString("package:" + packageName.toString()).object());
    intent.callObjectMethod("setData", "(Landroid/net/Uri;)Landroid/content/Intent;", uri.object());
    // Intent.FLAG_ACTIVITY_NEW_TASK
    intent.callObjectMethod("addFlags", "(I)Landroid/content/Intent;", 0x10000000);
    context.callMethod<void>("startActivity", "(Landroid/content/Intent;)V", intent.object());
#endif
}

/**
 * @brief 持久化SAF目录授权
 * @param uri 系统文件夹选择器返回的 content:// tree URI
 * @note 不持久化的话, 设备重启后授权会丢失, 文件树又只能看到目录
 */
void persistAndroidTreeUri (const QString &uri) {
#if defined(__ANDROID__)
    if (!uri.startsWith("content://"))
        return;
    // android.content.Intent.FLAG_GRANT_READ_URI_PERMISSION | FLAG_GRANT_WRITE_URI_PERMISSION
    constexpr jint readWriteFlags = 0x1 | 0x2;
    QJniObject androidUri = QJniObject::callStaticObjectMethod(
        "android/net/Uri", "parse", "(Ljava/lang/String;)Landroid/net/Uri;",
        QJniObject::fromString(uri).object());
    // Qt 6.8+/6.9 的 context() 返回类型化 QtJniTypes::Context(JObject<ContextTag>),
    // 可隐式转成 QJniObject, 再走经典 API 调 ContentResolver
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    QJniObject resolver = context.callObjectMethod("getContentResolver",
                                                   "()Landroid/content/ContentResolver;");
    resolver.callMethod<void>("takePersistableUriPermission",
                              "(Landroid/net/Uri;I)V", androidUri.object(), readWriteFlags);
#endif
}

/**
 * @brief 检查有无所有文件获取权限
 * @return 是否
 */
bool hasManageExternalStorage () {
#if defined(__ANDROID__)
    // isExternalStorageManager 返回 boolean, 必须用类型化 callStaticMethod<jboolean>,
    // callStaticObjectMethod 只能调返回对象的 Java 方法, 否则 ART 会直接 abort
    return QJniObject::callStaticMethod<jboolean>(
        "android/os/Environment",
        "isExternalStorageManager"
    );
#endif
    return false;
}

/**
 * @brief 复制Debug文本到剪贴板(仅安卓)
 */
void copyAndroidDebugText () {
    QApplication::clipboard()->setText(androidDebugText);
}

/**
 * @brief 获取文件夹访问权限(仅安卓)
 * @note 调起系统文件夹选择器(SAF), 持久化授权后把 content:// 路径复制到剪贴板
 */
void grantFolderPermission () {
    auto option = QFileDialog::Options();
    option |= QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks;
    const QString dir = QFileDialog::getExistingDirectory(nullptr, "选择文件夹以获取访问权限",
                                                          QDir::homePath(), option);
    if (dir.isEmpty())
        return;
    persistAndroidTreeUri(dir); // 持久化授权, 避免设备重启后授权丢失
    QApplication::clipboard()->setText(dir);
}
