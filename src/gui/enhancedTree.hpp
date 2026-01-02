#ifndef CHARTNAVIGATION_ENHANCEDTREE_HPP
#define CHARTNAVIGATION_ENHANCEDTREE_HPP

template <typename T>
concept NodeT = std::is_base_of_v<QTreeWidgetItem, T> || std::is_base_of_v<QTreeWidget, T>;

class Node;
template <typename NodeT>
void traverseRead (const QDir &folder, NodeT *parentNode, bool delNonePdf);


class Node final : public QTreeWidgetItem {
    public:
        Node () = default;
        Node (QString baseDir, const QString &name, bool isFolder);

        QString baseDir; // 文件或目录路径
        bool isFolder{}; // 类型(0文件1目录)
};

/**
 * @brief 递归新建树节点
 * @tparam NodeT 父节点类,根数或节点
 * @param folder 父文件夹
 * @param parentNode 父节点
 * @param delNonePdf 是否丢弃非PDF文件
 */
template <typename NodeT>
void traverseRead (const QDir &folder, NodeT *parentNode, const bool delNonePdf) {
    // 获取子文件夹/文件
    QStringList files = folder.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    std::vector<Node*> nodes;
    for (auto &fileName : files) {
        QString child{folder.filePath(fileName)};
        if (QFileInfo info(child); info.isFile()) {
            if (!child.endsWith(".pdf", Qt::CaseInsensitive) && delNonePdf)
                continue;
            nodes.emplace_back(new Node(child, info.completeBaseName(), false));
        } else
            nodes.emplace_back(new Node(child, info.completeBaseName(), true));
    }
    // 添加
    if (nodes.empty())
        nodes.emplace_back(new Node({}, {"[Empty!]"}, false));
    for (const auto node : nodes) {
        if constexpr (requires { parentNode->addTopLevelItem(node); })
            parentNode->addTopLevelItem(node);
        else
            parentNode->addChild(node);
    }
    // 遍历
    for (const auto node : nodes) {
        if (node->isFolder)
            traverseRead(node->baseDir, node, delNonePdf);
    }
}

#endif //CHARTNAVIGATION_ENHANCEDTREE_HPP
