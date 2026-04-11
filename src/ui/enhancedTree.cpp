#include "enhancedTree.hpp"
#include <QtConcurrent>
#include <QTemporaryDir>


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

Tree::Tree () {
    tempDir.setAutoRemove(false);
}

Tree::Tree (QWidget *parent) : QTreeWidget(parent) {
    tempDir.setAutoRemove(false);
    connect(this, &Tree::itemExpanded, this, [this](const QTreeWidgetItem *item) {
        for (int i = 0; i < item->childCount(); ++i) {
            const auto node = dynamic_cast<Node*>(item->child(i));
            if (node->isFolder)
                continue;
            loadThumb(node);
        }
    });
}

/**
 * @brief 加载某个根目录
 * @param folder 文件夹
 */
void Tree::loadFolder (const QString &folder) {
    // 前置准备
    children.clear();
    clear();
    // 运行
    traverseRead(folder, this);
    // 后置操作
    for (auto item : findItems("", Qt::MatchContains | Qt::MatchRecursive))
        children.insert(item);
}

/**
 * @brief 异步加载文件的缩略图
 * @param item 节点
 */
QCoro::Task<> Tree::loadThumb (QTreeWidgetItem *item) const {
    co_return;
    const auto node = dynamic_cast<Node*>(item);
    if (true)
        co_return;

    auto renderTask = [](const QString &path) -> QImage {
        constexpr QSize size{144, 192};
        if (path.endsWith(".pdf", Qt::CaseInsensitive)) {
            QPdfDocument doc;
            doc.load(path);
            const QImage transparentImg = doc.render(0, size);
            QImage finalImg(size, QImage::Format_RGB32);
            finalImg.fill(Qt::white);
            QPainter painter(&finalImg);
            painter.drawImage(0, 0, transparentImg);
            painter.end();
            return finalImg;
        } else {
            const QImage img(path);
            return img.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }
    };
    const QImage resultImage = co_await QtConcurrent::run(renderTask, node->baseDir);
    if (resultImage.isNull() || !children.contains(item)) {
        co_return;
    }
    item->setIcon(0, QIcon(QPixmap::fromImage(resultImage)));
}
