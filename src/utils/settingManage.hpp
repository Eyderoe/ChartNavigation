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
            inopEnumItem, // 谁写的钩子代码
            MainWindowGeo, // 主窗口尺寸
            isDarkTheme, // 暗色主题
        };
        Q_ENUM(ConfigKey)

        static SettingsManager& instance ();
        void set (ConfigKey key, const QVariant &value);
        QVariant get (ConfigKey key, const QVariant &defult = QVariant());
        void broadcast ();
    private:
        SettingsManager ();
        QSettings settings;
        QMap<QString, QVariant> cache;

        static QString key2String (ConfigKey key);
        static ConfigKey string2Key (const QString &keyStr);
        void writeSetting ();
    signals:
        void settingChanged (ConfigKey key, const QVariant &value);
};

#endif //CHARTNAVIGATION_SETTINGMANAGE_HPP
