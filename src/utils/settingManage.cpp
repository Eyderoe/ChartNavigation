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
 * @param notEmit 是否不发射
 */
void SettingsManager::set (const ConstKey key, const QVariant &value, const bool notEmit) {
    const QString keyName = key2String_const(key);
    const auto it = cache_const.find(keyName);
    if (it != cache_const.end()) {
        if (it.value() == value)
            return;
        it.value() = value;
    } else {
        cache_const[keyName] = value;
    }
    if (notEmit)
        return;
    emit settingChanged(key, value);
}

/**
 * @brief 获取键值对(存储值)
 * @param key 键
 * @param defaultValue 默认值
 * @return 值
 */
QVariant SettingsManager::get (const ConstKey key, const QVariant &defaultValue) {
    const QString keyName = key2String_const(key);
    const auto it = cache_const.find(keyName);
    if (it == cache_const.end()) {
        cache_const[keyName] = defaultValue;
        return defaultValue;
    } else {
        return it.value();
    }
}

/**
 * @brief 设置键值对(临时值)
 * @param key 键
 * @param value 值
 * @param notEmit 是否不发射
 */
void SettingsManager::set (const TempKey key, const QVariant &value, bool notEmit) {
    const QString keyName = key2String_temp(key);
    const auto it = cache_temp.find(keyName);
    if (it != cache_temp.end()) {
        if (it.value() == value)
            return;
        it.value() = value;
    } else {
        cache_temp[keyName] = value;
    }
    if (notEmit)
        return;
    emit settingChanged(key, value);
}

/**
 * @brief 获取键值对(临时值)
 * @param key 键
 * @param defaultValue 默认值
 * @return 值
 */
QVariant SettingsManager::get (TempKey key, const QVariant &defaultValue) {
    const QString keyName = key2String_temp(key);
    const auto it = cache_const.find(keyName);
    if (it == cache_const.end()) {
        cache_const[keyName] = defaultValue;
        return defaultValue;
    } else {
        return it.value();
    }
}

/**
 * @brief 广播一次所有键值对(存储值)
 */
void SettingsManager::broadcast () {
    for (int i = 0; i < QMetaEnum::fromType<SettingsManager::ConstKey>().keyCount(); ++i) {
        switch (const auto enumKey = static_cast<SettingsManager::ConstKey>(i)) {
            case inopEnumItem_constKey:
                break;

            case MainWindowGeo:
            case MainWidgetSta:
            case OptionWidgetGeo:
            case spliterSta:
                emit settingChanged(enumKey, get(enumKey, {}));
                break;

            case dataSource:
                emit settingChanged(enumKey, get(enumKey, 0));
                break;

            case planeFollowed:
            case stayFront:
                emit settingChanged(enumKey, get(enumKey, true));
                break;

            case scaleBarEnable:
                emit settingChanged(enumKey, get(enumKey, false));
                break;

            default:
                assert(false && "need to update switch case. [SettingsManager::broadcast]");
        }
    }
}

SettingsManager::SettingsManager () {
    for (const QString &key : settings.allKeys())
        cache_const[key] = settings.value(key);
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
    return {meta.valueToKey(key)};
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
        return ConstKey::inopEnumItem_constKey;
    return static_cast<ConstKey>(value);
}

/**
 * @brief 利用元系统将枚举变为字符串(临时值)
 * @param key 键
 * @return 字符串
 */
QString SettingsManager::key2String_temp (const TempKey key) {
    static QMetaEnum meta = QMetaEnum::fromType<TempKey>();
    return {meta.valueToKey(key)};
}

/**
 * @brief 利用元系统将字符串变为枚举(临时值)
 * @param keyStr 键
 * @return 字符串
 */
SettingsManager::TempKey SettingsManager::string2Key_temp (const QString &keyStr) {
    static QMetaEnum meta = QMetaEnum::fromType<TempKey>();
    int value = meta.keyToValue(keyStr.toUtf8().constData());
    if (value == -1)
        return TempKey::inopEnumItem_tempKey;
    return static_cast<TempKey>(value);
}

/**
 * @brief 写入设置到文件
 */
void SettingsManager::writeSetting () {
    for (auto [key, value] : cache_const.asKeyValueRange())
        settings.setValue(key, value);
}
