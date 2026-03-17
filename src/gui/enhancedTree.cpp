#include "enhancedTree.hpp"

/**
 * @brief 检查是否需要该文件
 * @return 需要丢弃
 */
bool shouldRetain (const QString &path, const DisplayFile displayMode) {
    switch (displayMode) {
        case DisplayFile::onlyPdf:
            if (!path.endsWith(".pdf", Qt::CaseInsensitive))
                return true;
        case DisplayFile::pdfWithPic:
            for (const auto &suffix : picFormat) {
                if (path.endsWith(suffix, Qt::CaseInsensitive))
                    return false;
            }
            return true;
        case DisplayFile::all:
            return false;
        default:
            return false;
    }
}

Node::Node (QString baseDir, const QString &name, const bool isFolder) : baseDir(std::move(baseDir)),
                                                                         isFolder(isFolder) {
    setText(0, name);
    if (isFolder)
        setForeground(0, QBrush(QColor(92, 145, 232))); // 很好看的蓝色
    else {
        if (this->baseDir.endsWith(".pdf", Qt::CaseInsensitive)) // 是PDF文件的话
            setForeground(0, QBrush(QColor(232, 135, 92))); // 很好看的橙色
        else
            setForeground(0, QBrush(QColor(55, 139, 53))); // 很好看的绿色
    }
}

Tree::Tree (QWidget *parent) : QTreeWidget(parent) {
    connect(this, &Tree::itemExpanded, this, [this](const QTreeWidgetItem *item) {
        for (int i = 0; i < item->childCount(); ++i) {
            const auto node = static_cast<Node*>(item->child(i));
            if (node->isFolder)
                continue;
            loadThumb(node);
        }
    });
}

/**
 * @brief 更改根目录操作
 * @param folder
 * @param mode
 */
void Tree::switchFolder (const QString &folder, const DisplayFile mode) {
    // 前置准备
    children.clear();
    clear();
    // 运行
    traverseRead(folder, this, mode);
    // 后置操作
    QList<QTreeWidgetItem*> allItems = findItems("", Qt::MatchContains | Qt::MatchRecursive);
    for (auto item : allItems)
        children.insert(item);
}

/**
 * @brief 异步加载文件的缩略图
 * @param item 节点
 */
QCoro::Task<> Tree::loadThumb (QTreeWidgetItem *item) const {
    const auto node = static_cast<Node*>(item);
    bool supportThumb{false};
    for (auto &suffix : picFormat) {
        if (node->baseDir.endsWith(suffix)) {
            supportThumb = true;
            break;
        }
    }
    if (!supportThumb)
        co_return;

    auto pic = [](const QString &path) -> QCoro::Task<QIcon> {
        if (path.endsWith(".pdf", Qt::CaseInsensitive)) {
            QPdfDocument doc;
            doc.load(path);
            const auto img=doc.render(0,QSize(48,64));
            co_return QIcon(QPixmap::fromImage(img));
        } else { // 正常图片
            const QPixmap originalPixmap(path);
            const QPixmap scaledPixmap = originalPixmap.scaled(QSize(48, 64), Qt::IgnoreAspectRatio,
                                                               Qt::SmoothTransformation);
            co_return QIcon(scaledPixmap);
        }
    };
    const auto result = co_await pic(node->baseDir);
    if (const bool stillAvailable = children.contains(item); !stillAvailable)
        co_return;
    item->setIcon(0, QIcon(result));
}
