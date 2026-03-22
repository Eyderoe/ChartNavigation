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
 * @brief 设置键值对
 * @param key 键
 * @param value 值
 */
void SettingsManager::set (const ConfigKey key, const QVariant &value) {
    const QString keyName = key2String(key);
    const auto it = cache.find(keyName);
    if (it != cache.end()) {
        if (it.value() == value)
            return;
        it.value() = value;
    } else {
        cache[keyName] = value;
    }
    emit settingChanged(key, value);
}

/**
 * @brief 获取键值对
 * @param key 键
 * @param defult 默认值
 * @return 值
 */
QVariant SettingsManager::get (const ConfigKey key, const QVariant &defult) {
    const QString keyName = key2String(key);
    const auto it = cache.find(keyName);
    if (it == cache.end()) {
        cache[keyName] = defult;
        return defult;
    } else {
        return it.value();
    }
}

/**
 * @brief 广播一次所有键值对
 */
void SettingsManager::broadcast () {
    for (auto [key, value] : cache.asKeyValueRange()) {
        const ConfigKey enumItem = string2Key(key);
        if (enumItem == ConfigKey::inopEnumItem)
            continue;
        emit settingChanged(enumItem, value);
    }
}

SettingsManager::SettingsManager () {
    for (const QString &key : settings.allKeys())
        cache[key] = settings.value(key);
}

/**
 * @brief 利用元系统将枚举变为字符串
 * @param key 键
 * @return 字符串
 */
QString SettingsManager::key2String (const ConfigKey key) {
    static QMetaEnum meta = QMetaEnum::fromType<ConfigKey>();
    return QString(meta.valueToKey(key));
}

/**
 * @brief 利用元系统将字符串变为枚举
 * @param keyStr 键
 * @return 字符串
 */
SettingsManager::ConfigKey SettingsManager::string2Key (const QString &keyStr) {
    static QMetaEnum meta = QMetaEnum::fromType<ConfigKey>();
    int value = meta.keyToValue(keyStr.toUtf8().constData());
    if (value == -1)
        return ConfigKey::inopEnumItem;
    return static_cast<ConfigKey>(value);
}

/**
 * @brief 写入设置到文件
 */
void SettingsManager::writeSetting () {
    for (auto [key, value] : cache.asKeyValueRange())
        settings.setValue(key, value);
}
