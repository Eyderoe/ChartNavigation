#include "settingManage.hpp"

/**
 * @brief 单例模式
 * @return 返回单例
 */
SettingsManager& SettingsManager::instance () {
    static SettingsManager inst;
    return inst;
}

/**
 * @brief 设置键值对(存储值)
 * @param key 键
 * @param value 值
 */
void SettingsManager::set (const ConstKey key, const QVariant &value) {
    const QString keyName = key2String_const(key);
    const auto it = cache.find(keyName);
    if (it != cache.end()) {
        if (it.value() == value)
            return;
        it.value() = value;
    } else {
        cache[keyName] = value;
    }
    emit constSettingChanged(key, value);
}

/**
 * @brief 获取键值对(存储值)
 * @param key 键
 * @param defult 默认值
 * @return 值
 */
QVariant SettingsManager::get (const ConstKey key, const QVariant &defult) {
    const QString keyName = key2String_const(key);
    const auto it = cache.find(keyName);
    if (it == cache.end()) {
        cache[keyName] = defult;
        return defult;
    } else {
        return it.value();
    }
}

/**
 * @brief 广播一次所有键值对(存储值)
 */
void SettingsManager::broadcast () {
    for (auto [key, value] : cache.asKeyValueRange()) {
        const ConstKey enumItem = string2Key_const(key);
        if (enumItem == ConstKey::inopEnumItem)
            continue;
        emit constSettingChanged(enumItem, value);
    }
}

SettingsManager::SettingsManager () {
    for (const QString &key : settings.allKeys())
        cache[key] = settings.value(key);
}

SettingsManager::~SettingsManager () {
    writeSetting();
}

/**
 * @brief 利用元系统将枚举变为字符串(存储值)
 * @param key 键
 * @return 字符串
 */
QString SettingsManager::key2String_const (const ConstKey key) {
    static QMetaEnum meta = QMetaEnum::fromType<ConstKey>();
    return QString(meta.valueToKey(key));
}

/**
 * @brief 利用元系统将字符串变为枚举(存储值)
 * @param keyStr 键
 * @return 字符串
 */
SettingsManager::ConstKey SettingsManager::string2Key_const (const QString &keyStr) {
    static QMetaEnum meta = QMetaEnum::fromType<ConstKey>();
    int value = meta.keyToValue(keyStr.toUtf8().constData());
    if (value == -1)
        return ConstKey::inopEnumItem;
    return static_cast<ConstKey>(value);
}

/**
 * @brief 写入设置到文件
 */
void SettingsManager::writeSetting () {
    for (auto [key, value] : cache.asKeyValueRange())
        settings.setValue(key, value);
}
