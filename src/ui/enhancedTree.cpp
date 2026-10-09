#include <QtConcurrentRun>
#include <QProcess>
#include <rapidhash.h>

#include "enhancedTree.hpp"
#include "chartColor.hpp"
#include "json.hpp"

#include "android/android.hpp"
#include "utils/constValue.hpp"

namespace {
QHash<QString, QString> readChartNames (const QString &path, const QString &icao) {
    QFile file(path);
    if (!file.exists())
        return {};
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << path << " 无法读取";
        return {};
    }
    QHash<QString, QString> names;
    try {
        const auto data = nlohmann::json::parse(file.readAll().toStdString());
        const auto *charts = &data;
        if (data.is_object()) {
            const auto it = data.find("charts");
            if (it == data.end())
                return {};
            charts = &it.value();
        }
        if (!charts->is_array())
            return {};
        for (const auto &chart : *charts) {
            if (!chart.is_object())
                continue;
            const auto index = chart.find("index_number"), name = chart.find("name");
            if (index == chart.end() || name == chart.end() || !index->is_string() || !name->is_string())
                continue;
            const QString indexText = QString::fromStdString(index->get<std::string>());
            const QString nameText = QString::fromStdString(name->get<std::string>());
            if (indexText.trimmed().isEmpty() || nameText.trimmed().isEmpty())
                continue;
            names.insert(icao + "-" + indexText, nameText);
        }
    } catch (const nlohmann::json::exception &) {
        qWarning() << path << " 解析失败";
        return {};
    }
    return names;
}
}


Node::Node (QString baseDir, const QString &name, const bool isFolder) : baseDir(std::move(baseDir)),
                                                                         isFolder(isFolder) {
    setText(0, name);
    if (isFolder)
        color = {92, 145, 232}; // 很好看的蓝色
    else {
        if (this->baseDir.endsWith(".pdf", Qt::CaseInsensitive)) { // 是PDF文件的话
            color = {232, 135, 92}; // 很好看的橙色
            isPdf = true;
        } else {
            color = {55, 139, 53}; // 很好看的绿色
        }
    }
    setForeground(0, color);
}

/**
 * @brief 获取哈希,未计算则先计算
 * @return 哈希值(失败为0)
 */
uint64_t Node::getHash () {
    if (isFolder)
        return 0;
    if (hash)
        return hash;
    // 否则开始计算
    if constexpr (platform == MultiPlatform::androidOS) {
        if (!androidMasterAccess()) {
            QString temp = QDateTime::currentDateTime().toString("MMdd") + baseDir;
            return rapidhash(temp.constData(), temp.size() * sizeof(QChar));
        }
    }
    QFile file(baseDir);
    if (!file.open(QIODevice::ReadOnly))
        return 0;
    const qint64 fileSize = file.size();
    if (fileSize == 0)
        return 0;
    const QByteArray buffer = file.read(fileSize);
    if (buffer.isEmpty())
        return 0;
    hash = rapidhash(buffer.constData(), buffer.size());
    return hash;
}

void Node::switchColor () {
    isRawColor = !isRawColor;
    if (isRawColor)
        setForeground(0, color);
    else
        setForeground(0, QColor{255, 0, 0});
}

Tree::Tree (QWidget *parent) : QTreeWidget(parent) {
    const SettingsManager &ins = SettingsManager::instance();
    // 缓存目录
    constexpr int cacheFileMax = (platform == MultiPlatform::androidOS) ? 20 : 1000;
    cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    qDebug() << "Cache path: " << cacheDir.path();
    if (!cacheDir.exists() && !cacheDir.mkpath(".")) {
        qDebug() << "缓存目录创建失败";
    } else if (cacheDir.count() > cacheFileMax) {
        shouldClean = true;
    }
    // 连接
    connect(this, &Tree::itemExpanded, this, &Tree::expand);
    connect(this, &Tree::itemCollapsed, this, &Tree::collapse);
    connect(this, &QTreeWidget::itemPressed, this, [&](QTreeWidgetItem *item, const int column) {
        const auto node = dynamic_cast<Node*>(item);
        if (qApp->mouseButtons() & Qt::RightButton)
            node->switchColor();
    });

    connect(&ins, qOverload<SettingsManager::ConstKey, const QVariant&>(&SettingsManager::settingChanged), this,
            [this](const SettingsManager::ConstKey key, const QVariant &val) {
                switch (key) {
                    case SettingsManager::showThumb:
                        showThumbPic = val.toBool();
                        for (const auto node : visibleNodes)
                            loadThumb(node);
                        break;
                    case SettingsManager::darkChartStyle:
                        if (darkTheme)
                            for (const auto node : visibleNodes)
                                loadThumb(node);
                        break;
                    default:
                        break;
                }
            });
    connect(&ins, qOverload<SettingsManager::TempKey, const QVariant&>(&SettingsManager::settingChanged), this,
            [this](const SettingsManager::TempKey key, const QVariant &val) {
                switch (key) {
                    case SettingsManager::isDarkTheme:
                        darkTheme = val.toBool();
                        for (const auto node : visibleNodes)
                            loadThumb(node);
                        break;
                    default:
                        break;
                }
            });
}

Tree::~Tree () {
    if (!shouldClean || !cacheDir.exists())
        return;
    cacheDir.removeRecursively();
}

/**
 * @brief 加载某个根目录
 * @param folder 文件夹
 */
void Tree::loadFolder (const QString &folder) {
    // 前置准备
    visibleNodes.clear();
    clear();
    // 运行
    const int count = traverseRead(folder, this);
    updateFileNames();
    visibleNodes.reserve(count);
    // 后置操作
    for (int i = 0; i < topLevelItemCount(); ++i) {
        const auto item = dynamic_cast<Node*>(topLevelItem(i));
        if (item->isFolder)
            continue;
        visibleNodes.insert(item);
        loadThumb(item);
    }
}

/**
 * @brief 复杂样式按 ICAO-index_number 查找航图名称，保留节点的原始文件路径。
 */
void Tree::updateFileNames () {
    SettingsManager &settings = SettingsManager::instance();
    const int style = settings.get(SettingsManager::fileTreeStyle, 0).toInt();
    if (style != 1 && style != 2)
        return;
    const QString mappingFolder = settings.get(SettingsManager::dataFolder, "").toString();
    if (mappingFolder.isEmpty() || !QDir(mappingFolder).exists())
        return;
    const QDir mappingDir(mappingFolder);
    // 每次构建文件树，每个机场只读取一次；缺失或无效的文件也缓存空结果。
    QHash<QString, QHash<QString, QString>> airportNames;
    for (QTreeWidgetItemIterator it(this); *it; ++it) {
        auto *node = dynamic_cast<Node*>(*it);
        if (!node || node->isFolder || !node->isPdf)
            continue;
        const QString baseName = QFileInfo(node->baseDir).completeBaseName();
        if (baseName.size() <= 5 || baseName.at(4) != '-')
            continue;
        const QString icao = baseName.left(4);
        if (!airportNames.contains(icao))
            airportNames.insert(icao, readChartNames(mappingDir.filePath(icao + ".Tnavi"), icao));
        const auto &names = airportNames[icao];
        if (const auto name = names.constFind(baseName); name != names.cend())
            node->setText(0, style == 1 ? icao + "-" + name.value() : name.value());
    }
}

/**
 * @brief 展开某节点时,对子节点的处理
 * @param item 父节点
 */
void Tree::expand (const QTreeWidgetItem *item) {
    for (int i = 0; i < item->childCount(); ++i) {
        const auto child = dynamic_cast<Node*>(item->child(i));
        if (child->isFolder || !child->isPdf)
            continue;
        visibleNodes.insert(child);
        loadThumb(child);
    }
}

/**
 * @brief 折叠某节点时,对子节点的处理
 * @param item 父节点
 */
void Tree::collapse (const QTreeWidgetItem *item) {
    for (int i = 0; i < item->childCount(); ++i) {
        const auto child = dynamic_cast<Node*>(item->child(i));
        if (child->isFolder || !child->isPdf)
            continue;
        visibleNodes.erase(child);
        child->setIcon(0, {});
    }
}

/**
 * @brief 异步加载文件的缩略图
 * @param item 节点
 * @note 只要进入这个函数,缩略图就一定会生成.然后每次展开目录都会进来一次
 */
QCoro::Task<> Tree::loadThumb (Node *item) const {
    auto renderTask = [](const QString &pdfPath, const QString &thumbPath) -> QImage {
        // 渲染图片
        QPdfDocument doc;
        doc.load(pdfPath);
        const QSizeF pageSize = doc.pagePointSize(0);
        const bool portrait = pageSize.height() > pageSize.width();
        const QSize size = portrait ? QSize{288, 384} : QSize{288, 256};
        const QImage transparentImg = doc.render(0, size);
        // 进一步处理
        QImage finalImg(size, QImage::Format_RGB32);
        finalImg.fill(Qt::white);
        QPainter painter(&finalImg);
        painter.drawImage(0, 0, transparentImg);
        painter.end();
        if (!portrait) {
            QTransform transform;
            transform.rotate(-90);
            finalImg = finalImg.transformed(transform);
        }
        // 保存并返回
        if (!finalImg.save(thumbPath))
            qDebug() << "缩略图保存失败";
        return finalImg;
    };
    auto getHash = [](Node *node) -> std::string {
        return std::format("{:#x}", node->getHash());
    };

    // 没有选择加载缩略图 先删除缩略图
    if (!showThumbPic) {
        item->setIcon(0, {});
        if constexpr (platform == MultiPlatform::androidOS) {
            if (androidMasterAccess())
                co_return ;
        }
    }
    // 检查目录下有没有
    std::string hash;
    if (item->hash)
        hash = std::format("{:#x}", item->hash);
    else
        hash = co_await QtConcurrent::run(getHash, item);
    const QString filePath = cacheDir.filePath(QString::fromStdString(hash + ".jpg"));
    // 没有的话就先生成缩略图
    QImage resultImage;
    if (!QFile::exists(filePath))
        resultImage = co_await QtConcurrent::run(renderTask, item->baseDir, filePath);
    // 没有选择加载缩略图 返回
    if (!showThumbPic)
        co_return ;
    // 然后再加载
    if (!visibleNodes.contains(item)) // 已经不显示该节点了
        co_return;
    if (resultImage.isNull()) {
        resultImage = QImage(filePath);
        if (resultImage.isNull())
            co_return;
    }
    if (darkTheme)
        applyDarkChartTheme(resultImage, SettingsManager::instance().get(SettingsManager::darkChartStyle, 0).toInt());
    item->setIcon(0, QIcon(QPixmap::fromImage(resultImage)));
}
