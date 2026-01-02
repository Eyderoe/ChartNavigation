#include "enhancedTree.hpp"

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
