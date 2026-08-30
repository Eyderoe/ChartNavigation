#ifndef CHARTNAVIGATION_SETTINGMANAGE_HPP
#define CHARTNAVIGATION_SETTINGMANAGE_HPP

#include <QObject>
#include <QVariant>
#include <QMap>
#include <QSettings>
#include <vector>
#include <initializer_list>

class CycleIdx;
class SettingsManager;


class CycleIdx {
    public:
        explicit CycleIdx (int length);
        explicit CycleIdx (std::initializer_list<int> list);
        int next ();
    private:
        std::vector<int> list;
        int loc{0};
};

class SettingsManager : public QObject {
        Q_OBJECT
    public:
        enum ConstKey { // 要持续性存储的
            inopEnumItem_constKey, // 兜底的怪东西

            MainWindowGeo, // 主窗口尺寸 ByteArray
            MainWindowSta, // 主窗口状态 ByteArray
            OptionWidgetGeo, // 设置窗口尺寸 ByteArray
            spliterSta, // 分割器状态 ByteArray
            stayFront, // 置顶窗口 bool
            scaleBarEnable, // 启用缩放条 bool

            onlyDisplayPdf, // 是否只显示PDF bool
            showThumb, // 显示缩略图 bool

            dataSource, // 数据源 SimulatorSource(int)
            planeFollowed, // 居中飞机 bool
            tcasRange, // TCAS显示范围 TcasMode(int)
            infoMode, // 飞行器信息 InfoMode(int)
            showTrail, // 显示飞行器航迹 bool
            useCalGeoHeading, // 使用计算航向 bool
            unitConvert, // 选择的单位 string: "0 3 1 1"

            chartFolder, // 航图文件夹 String
            dataFolder, // 数据文件夹 String
            globeFolder, // 高程文件夹 String
            airacPath, // 导航数据路径 String

            debugStoreData, // [Debug]保存数据 bool
            debugReplayData, // [Debug]回放数据 bool
            debugReplayFile, // [Debug]回放数据路径 String
        };
        enum TempKey { // 仅在程序运行时存在的
            inopEnumItem_tempKey, // 兜底的怪东西
            isDarkTheme, // 暗色主题 bool
            affineError, // 仿射误差 double [不可用时为nan]
            affineQuality, // 仿射质量 AffineQuality(int)
            simuConnect, // 模拟器连接 bool
            latitu, // 纬度 double
            longitu, // 经度 double
            altRelat, // 高度(安卓)/离地高 int
            suicide, // 自刎归天 bool
        };
        Q_ENUM(ConstKey)
        Q_ENUM(TempKey)

        static SettingsManager& instance ();
        void broadcast ();
        void writeSetting ();

        void set (ConstKey key, const QVariant &value, bool notEmit = false);
        QVariant get (ConstKey key, const QVariant &defaultValue = QVariant());
        void set (TempKey key, const QVariant &value, bool notEmit = false);
        QVariant get (TempKey key, const QVariant &defaultValue = QVariant());
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


inline CycleIdx mainPage(3);

#endif //CHARTNAVIGATION_SETTINGMANAGE_HPP
