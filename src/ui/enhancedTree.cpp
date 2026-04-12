#include <QtConcurrent>
#include <rapidhash.h>

#include "enhancedTree.hpp"
#include "utils/constValue.hpp"


Node::Node (QString baseDir, const QString &name, const bool isFolder) : baseDir(std::move(baseDir)),
                                                                         isFolder(isFolder) {
    setText(0, name);
    if (isFolder)
        setForeground(0, QBrush(QColor(92, 145, 232))); // 很好看的蓝色
    else {
        if (this->baseDir.endsWith(".pdf", Qt::CaseInsensitive)) { // 是PDF文件的话
            setForeground(0, QBrush(QColor(232, 135, 92))); // 很好看的橙色
            isPdf = true;
        } else {
            setForeground(0, QBrush(QColor(55, 139, 53))); // 很好看的绿色
        }
    }
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

Tree::Tree (QWidget *parent) : QTreeWidget(parent) {
    const SettingsManager &ins = SettingsManager::instance();
    // 缓存目录
    cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    qDebug() << "Cache path: " << cacheDir.path();
    if (!cacheDir.exists() && !cacheDir.mkpath("."))
        qDebug() << "缓存目录创建失败";
    cleanCacheDir();
    // 连接
    connect(this, &Tree::itemExpanded, this, &Tree::expand);

    connect(&ins, qOverload<SettingsManager::ConstKey, const QVariant&>(&SettingsManager::settingChanged), this,
            [this](const SettingsManager::ConstKey key, const QVariant &val) {
                switch (key) {
                    case SettingsManager::showThumb:
                        showThumbPic = val.toBool();
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
 * @note 只要进入这个函数,缩略图就一定会生成
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

    // 先检查目录下面有没有
    const std::string hash = co_await QtConcurrent::run(getHash, item) + ".jpg";
    const QString filePath = cacheDir.filePath(QString::fromStdString(hash));
    // 没有的话就先生成缩略图
    QImage resultImage;
    if (!QFile::exists(filePath))
        resultImage = co_await QtConcurrent::run(renderTask, item->baseDir, filePath);
    if (!showThumbPic) // 没有选择加载缩略图
        co_return ;
    // 然后再加载
    if (resultImage.isNull())
        resultImage = QImage(filePath);
    if (resultImage.isNull() || !visibleNodes.contains(item)) // 已经不显示该节点了
        co_return;
    if (darkTheme)
        resultImage.invertPixels();
    item->setIcon(0, QIcon(QPixmap::fromImage(resultImage)));
}

void Tree::cleanCacheDir () {
    if (cacheDir.count() > 2000)
        shouldClean = true;
}
