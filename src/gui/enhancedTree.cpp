#include "enhancedTree.hpp"

Node::Node (QString baseDir, const QString &name, const int sidebarLoc, const bool isFolder) :
    baseDir(std::move(baseDir)), sidebarLoc(sidebarLoc), isFolder(isFolder) {
    setText(0, name);
    if (isFolder)
        setForeground(0, QBrush(QColor(92, 145, 232))); // 很好看的蓝色
    else
        setForeground(0, QBrush(QColor(232, 135, 92))); // 很好看的橙色
}