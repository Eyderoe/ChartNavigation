#ifndef CHARTNAVIGATION_SETTINGMANAGE_HPP
#define CHARTNAVIGATION_SETTINGMANAGE_HPP

#include <QObject>
#include <QVariant>
#include <QMap>
#include <QSettings>

class SettingsManager : public QObject {
        Q_OBJECT
    public:
        enum ConfigKey {
            inopEnumItem, // 兜底的怪东西
            MainWindowGeo, // 主窗口尺寸 ByteArray
            isDarkTheme, // 暗色主题 bool
        };
        Q_ENUM(ConfigKey)

        static SettingsManager& instance ();
        void set (ConfigKey key, const QVariant &value);
        QVariant get (ConfigKey key, const QVariant &defult = QVariant());
        void broadcast ();
        void writeSetting ();
    private:
        SettingsManager ();
        QSettings settings;
        QMap<QString, QVariant> cache;

        static QString key2String (ConfigKey key);
        static ConfigKey string2Key (const QString &keyStr);
    signals:
        void settingChanged (ConfigKey key, const QVariant &value);
};

#endif //CHARTNAVIGATION_SETTINGMANAGE_HPP
