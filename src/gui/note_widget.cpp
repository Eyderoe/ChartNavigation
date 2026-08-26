#include "note_widget.hpp"
#include "ui_note_widget.h"

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QPixmap>

#include <algorithm>
#include <numbers>

#include "services/settingManage.hpp"
#include "utils/stringProcess.hpp"
#include "utils/constValue.hpp"


note_widget::note_widget (QWidget *parent) : QWidget(parent), ui(new Ui::note_widget) {
    ui->setupUi(this);
    // 标准化
    initConnect();
    expandComboBox(ui->comboBox);
    QTimer::singleShot(0, this, [this] {
        const auto piece = static_cast<int>(width() / 10.0);
        ui->splitter->setSizes({piece * 6, piece * 4});
        ui->presetStackWidget->setCurrentIndex(0);
    });
    // 各页面
    initGraphic();
    initConversation();
    initUnit();
    initAlfaTable();
}

note_widget::~note_widget () {
    delete ui;
}

void note_widget::fitChinaFlightLevelToWidth () const {
    const QSize viewportSize = ui->imageGraphicsView->viewport()->size();
    if (viewportSize.width() <= 0)
        return;
    const QRectF sceneRect = flightLevelScene->sceneRect();
    if (sceneRect.width() <= 0)
        return;
    const double baseScale = static_cast<double>(viewportSize.width()) / sceneRect.width();
    ui->imageGraphicsView->resetTransform();
    ui->imageGraphicsView->scale(baseScale, baseScale);
    ui->imageGraphicsView->horizontalScrollBar()->setValue(ui->imageGraphicsView->horizontalScrollBar()->minimum());
}

bool note_widget::eventFilter (QObject *watched, QEvent *event) {
    if (watched == ui->imageGraphicsView->viewport() && event->type() == QEvent::Resize) {
        fitChinaFlightLevelToWidth();
    }
    return QWidget::eventFilter(watched, event);
}

void note_widget::initConnect () {
    const auto &setting = SettingsManager::instance();
    // 存储设置
    connect(&setting, qOverload<SettingsManager::ConstKey, const QVariant&>(&SettingsManager::settingChanged), this,
            [this](const SettingsManager::ConstKey key, const QVariant &val) {
                switch (key) {
                    case SettingsManager::unitConvert: {
                        // 由于只有一个设置 干脆写这算了
                        const auto rawStr = val.toString().toStdString();
                        const auto strList = split(rawStr);
                        auto comboBoxList = ui->unitConvert->findChildren<QComboBox*>();
                        const auto maxNum = std::min(strList.size(), static_cast<size_t>(comboBoxList.size()));
                        for (auto i = 0; i < maxNum; ++i) {
                            int idx = std::stoi(std::string(strList[i]));
                            comboBoxList[i]->setCurrentIndex(std::min(idx, comboBoxList[i]->count()));
                        }
                        break;
                    }
                    default:
                        break;
                }
            });
    // 临时设置
    connect(&setting, qOverload<SettingsManager::TempKey, const QVariant&>(&SettingsManager::settingChanged), this,
            [this](const SettingsManager::TempKey key, const QVariant &val) {
                switch (key) {
                    case SettingsManager::suicide:
                        saveUnit(val.toBool());
                        break;
                    default:
                        break;
                }
            });
}

void note_widget::initGraphic () {
    static QPixmap pixmap(":/preset/resources/documents/chinaFlightLevel.png");
    flightLevelScene = new QGraphicsScene(this);
    flightLevelScene->addPixmap(pixmap);
    flightLevelScene->setSceneRect(pixmap.rect());
    ui->imageGraphicsView->setScene(flightLevelScene);
    ui->imageGraphicsView->viewport()->installEventFilter(this);
}

void reviseText (QTextDocument *document, const int textSize) {
    QTextCursor cursor(document);
    cursor.select(QTextCursor::Document);
    QTextCharFormat format;
    format.setFontPointSize(textSize);
    format.setFontFamilies({QFontDatabase::systemFont(QFontDatabase::FixedFont).family()});
    cursor.mergeCharFormat(format);
}

void note_widget::initConversation () const {
    // 多平台字号不一致性
    const int textSize = QFont().pointSize();
    reviseText(ui->conversationTextEdit->document(), textSize);
}

/**
 * @brief 通过当前触发对象,获取同侧和异侧
 * @param name 对象名称
 * @return [另一侧 另一侧 同侧 同侧]
 */
std::tuple<QComboBox*, QLineEdit*, QComboBox*, QLineEdit*> note_widget::getUnit (const QString &name) const {
    static QString sep{"_"};
    const auto groupName = split(name, sep)[1];
    const QString cb_pre = "comboBox_" + groupName + "_";
    const QString le_pre = "lineEdit_" + groupName + "_";
    const QString end = name[name.size() - 1];
    const QString oppoEnd = (end == "1") ? "2" : "1";
    // 开找
    auto cbo = ui->unitConvert->findChild<QComboBox*>(cb_pre + oppoEnd);
    auto *cb = ui->unitConvert->findChild<QComboBox*>(cb_pre + end);
    auto *leo = ui->unitConvert->findChild<QLineEdit*>(le_pre + oppoEnd);
    auto *le = ui->unitConvert->findChild<QLineEdit*>(le_pre + end);
    return {cbo, leo, cb, le};
}

void note_widget::saveUnit (const bool save) const {
    if (!save)
        return;
    SettingsManager &manager = SettingsManager::instance();
    auto comboBoxList = ui->unitConvert->findChildren<QComboBox*>();
    std::vector<std::string> indexList;
    for (const auto item : comboBoxList)
        indexList.push_back(std::format("{}", item->currentIndex()));
    const std::string finalStr = join(indexList, " ");
    manager.set(SettingsManager::unitConvert, QString::fromStdString(finalStr), true);
}

void note_widget::initUnit () const {
    // 单位换算这部分写的太棒了简直, 使用了各种奇技淫巧, 而且没用ai
    // 复制模型
    for (const auto comboBox : ui->unitConvert->findChildren<QComboBox*>(QRegularExpression("^comboBox_.*_2$"))) {
        QString name = comboBox->objectName().replace("_2", "_1");
        comboBox->setModel(ui->unitConvert->findChild<QComboBox*>(name)->model());
    }
    // 删除指示
    for (const auto *combo : ui->unitConvert->findChildren<QComboBox*>())
        expandComboBox(combo);
    // 关联所有combobox和lineeidt
    for (const QComboBox *cb : ui->unitConvert->findChildren<QComboBox*>())
        connect(cb, &QComboBox::activated, this, &note_widget::unitConvertChange);
    for (const QLineEdit *le : ui->unitConvert->findChildren<QLineEdit*>())
        connect(le, &QLineEdit::editingFinished, this, &note_widget::unitConvertChange);
}

void note_widget::initAlfaTable () const {
    const int textSize = static_cast<int>(QFont().pointSize() * 1.5);
    reviseText(ui->alfaTextEdit->document(), textSize);
}

void note_widget::on_comboBox_activated (const int index) const {
    ui->presetStackWidget->setCurrentIndex(index);
}

constexpr std::array<std::pair<std::string_view, double>, 17> makeUnitScale () {
    std::array<std::pair<std::string_view, double>, 17> result{
        {
            // 距离基准 m
            {"nmi2m", 1852},
            {"km2m", 1000},
            {"m2m", 1},
            {"ft2m", 0.3048},
            // 重量基准 kg
            {"t2kg", 1000},
            {"kg2kg", 1},
            {"lb2kg", 0.45359237},
            // 速度基准 km/h
            {"mps2kmph", 3.6},
            {"kmph2kmph", 1},
            {"kn2kmph", 1.852},
            // 压力基准 hpa
            {"bar2hPa", 1000},
            {"psi2hPa", 68.9376},
            {"inHg2hPa", 33.8639},
            {"hPa2hPa", 1},
            // 坡度基准 ft/nmi
            {"percent2ftpnmi", 60.76115},
            {"degree2ftpnmi", -1},
            {"ftpnmi2ftpnmi", 1},
        }
    };
    std::ranges::sort(result, {}, [](const auto &p) { return p.first; });
    return result;
}
constexpr auto unitScale = makeUnitScale();
double getScale (QString text) {
    const auto idx = text.replace("/", "p").toStdString();
    const auto it = std::ranges::lower_bound(unitScale, idx, {}, [](const auto &p) { return p.first; });
    return it->second;
}

std::tuple<double, double> gradient2base (const double theta) { // 度°转换到基准的话需要tan
    return {1, std::tan(theta * std::numbers::pi / 180) * 6076.115};
}
std::tuple<double, double> base2gradient (const double theta) { // 基准转换到度°的话需要atan
    return {1, 1 / (std::atan(theta / 6076.115) * 180 / std::numbers::pi)};
}
/**
 * @brief 大统一,单位转换
 * @note 值优先改变对侧,单位优先改变同侧.
 */
void note_widget::unitConvertChange () const {
    QObject *senderObj = sender();
    auto [cbo,leo,cb,le] = getUnit(senderObj->objectName());
    if (qobject_cast<QComboBox*>(senderObj) == nullptr) {
        std::swap(cbo, cb);
        std::swap(leo, le);
    } else {
        if constexpr (platform == MultiPlatform::androidOS)
            saveUnit();
    }
    // 可能的交换方向
    bool isNum;
    double value = leo->text().toDouble(&isNum);
    if (!isNum) { // 对侧不是数字
        value = le->text().toDouble(&isNum);
        if (!isNum) // 同侧也不是数字
            return;
        std::swap(cbo, cb);
        std::swap(leo, le);
    }
    // 开始计算
    double oScale = getScale(cbo->currentText());
    if (oScale == -1)
        std::tie(value, oScale) = gradient2base(value);
    double baseValue = value * oScale;
    double scale = getScale(cb->currentText());
    if (scale == -1)
        std::tie(baseValue, scale) = base2gradient(baseValue);
    value = baseValue / scale;
    le->setText(QString::asprintf("%.2f", value));
}
