#ifndef CHARTNAVIGATION_ENHANCEDTREE_HPP
#define CHARTNAVIGATION_ENHANCEDTREE_HPP


#include <qcoro/QCoroCore>


enum class DisplayFile;
class Node;
class Tree;

template <typename T>
concept NodeT = std::is_base_of_v<QTreeWidgetItem, T> || std::is_base_of_v<QTreeWidget, T>;

bool shouldRetain (const QString &path, DisplayFile displayMode);
template <typename NodeT>
void traverseRead (const QDir &folder, NodeT *parentNode, DisplayFile displayMode, int depth = 0);

const static std::set<QString> picFormat{".pdf", ".png", ".jpg", ".jpeg"};

enum class DisplayFile:int {
    onlyPdf, pdfWithPic, all
};

class Node final : public QTreeWidgetItem {
    public:
        Node () = default;
        Node (QString baseDir, const QString &name, bool isFolder);

        QString baseDir; // 文件或目录路径
        bool isFolder{}; // 类型(0文件1目录)
};

class Tree final : public QTreeWidget {
    public:
        Tree ();
        explicit Tree (QWidget *parent = nullptr);
        void switchFolder(const QString &folder,DisplayFile mode);
    private:
        QTemporaryDir tempDir;
        std::set<QTreeWidgetItem*> children;

        QCoro::Task<> loadThumb (QTreeWidgetItem *item) const;
};


/**
 * @brief 递归新建树节点
 * @tparam NodeT 父节点类,根树或节点
 * @param folder 父文件夹
 * @param parentNode 父节点
 * @param displayMode 文件筛选器
 * @param depth 当前深度,最大深度4
 */
template <typename NodeT>
void traverseRead (const QDir &folder, NodeT *parentNode, const DisplayFile displayMode, const int depth) {
    if (depth > 4)
        return;
    // 获取子文件夹/文件
    QStringList files = folder.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    std::vector<Node*> nodes;
    for (auto &fileName : files) {
        QString child{folder.filePath(fileName)};
        if (QFileInfo info(child); info.isFile()) {
            // 文件后缀审查
            if (const bool ignore = shouldRetain(child, displayMode); ignore)
                continue;
            // 添加到节点集
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
            traverseRead(node->baseDir, node, displayMode, depth + 1);
    }
}

#endif //CHARTNAVIGATION_ENHANCEDTREE_HPP
