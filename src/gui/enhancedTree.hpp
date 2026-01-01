#ifndef CHARTNAVIGATION_ENHANCEDTREE_HPP
#define CHARTNAVIGATION_ENHANCEDTREE_HPP

template <typename T>
concept NodeT = std::is_base_of_v<QTreeWidgetItem, T> || std::is_base_of_v<QTreeWidget, T>;

class Node;
template <typename NodeT>
void traverseRead (const QDir &folder, NodeT *parentNode);


class Node final : public QTreeWidgetItem {
    public:
        Node () = default;
        Node (QString baseDir, const QString &name, int sidebarLoc, bool isFolder);

        QString baseDir; // 文件或目录路径
        int sidebarLoc{}; // 侧边栏位置
        bool isFolder{}; // 类型(0文件1目录)
};

/**
 * @brief 递归新建树节点
 * @tparam NodeT 父节点类,根数或节点
 * @param folder 父文件夹
 * @param parentNode 父节点
 */
template <typename NodeT>
void traverseRead (const QDir &folder, NodeT *parentNode) {
    // 获取子文件夹/文件
    QStringList files = folder.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    std::vector<Node*> nodes;
    for (auto &fileName : files) {
        QString child{folder.filePath(fileName)};
        if (QFileInfo info(child); info.isFile())
            nodes.emplace_back(new Node(child,info.completeBaseName(),0,false));
        else
            nodes.emplace_back(new Node(child,info.completeBaseName(),0,true));
    }
    // 添加
    for (const auto node : nodes) {
        if constexpr (requires { parentNode->addTopLevelItem(node); })
            parentNode->addTopLevelItem(node);
        else
            parentNode->addChild(node);
    }
    // 遍历
    for (const auto node : nodes) {
        if (node->isFolder)
            traverseRead(node->baseDir, node);
    }
}

#endif //CHARTNAVIGATION_ENHANCEDTREE_HPP
