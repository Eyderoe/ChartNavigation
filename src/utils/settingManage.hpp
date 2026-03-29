#ifndef CHARTNAVIGATION_SETTINGMANAGE_HPP
#define CHARTNAVIGATION_SETTINGMANAGE_HPP

#include <QObject>
#include <QVariant>
#include <QMap>
#include <QSettings>

class SettingsManager : public QObject {
        Q_OBJECT
    public:
        enum ConstKey {
            inopEnumItem, // 兜底的怪东西
            MainWindowGeo, // 主窗口尺寸 ByteArray
            isDarkTheme, // 暗色主题 bool
            dataSource, // 数据源 int
        };
        enum TempKey {
        };
        Q_ENUM(ConstKey)
        Q_ENUM(TempKey)

        static SettingsManager& instance ();
        void set (ConstKey key, const QVariant &value);
        QVariant get (ConstKey key, const QVariant &defult = QVariant());
        void broadcast ();
        void writeSetting ();
    private:
        SettingsManager ();
        ~SettingsManager () override;
        QSettings settings;
        QMap<QString, QVariant> cache;

        static QString key2String_const (ConstKey key);
        static ConstKey string2Key_const (const QString &keyStr);
    signals:
        void constSettingChanged (ConstKey key, const QVariant &value);
};

#endif //CHARTNAVIGATION_SETTINGMANAGE_HPP
