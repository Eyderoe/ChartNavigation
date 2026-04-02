#ifndef CHARTNAVIGATION_SETTINGMANAGE_HPP
#define CHARTNAVIGATION_SETTINGMANAGE_HPP

#include <QObject>
#include <QVariant>
#include <QMap>
#include <QSettings>

class SettingsManager : public QObject {
        Q_OBJECT
    public:
        enum ConstKey { // 要持续性存储的
            inopEnumItem_constKey, // 兜底的怪东西
            MainWindowGeo, // 主窗口尺寸 ByteArray
            dataSource, // 数据源 SimulatorSource(int)
        };
        enum TempKey { // 仅在程序运行时存在的
            inopEnumItem_tempKey, // 兜底的怪东西
            isDarkTheme, // 暗色主题 bool
        };
        Q_ENUM(ConstKey)
        Q_ENUM(TempKey)

        static SettingsManager& instance ();
        void broadcast ();
        void writeSetting ();

        void set (ConstKey key, const QVariant &value);
        QVariant get (ConstKey key, const QVariant &defult = QVariant());
        void set (TempKey key, const QVariant &value);
        QVariant get (TempKey key, const QVariant &defult = QVariant());
    private:
        SettingsManager ();
        ~SettingsManager () override;
        QSettings settings;
        QMap<QString, QVariant> cache_const, cache_temp;

        static QString key2String_const (ConstKey key);
        static ConstKey string2Key_const (const QString &keyStr);
        static QString key2String_temp (TempKey key);
        static TempKey string2Key_temp (const QString &keyStr);
    signals:
        void settingChanged (ConstKey key, const QVariant &value);
        void settingChanged (TempKey key, const QVariant &value);
};

#endif //CHARTNAVIGATION_SETTINGMANAGE_HPP
