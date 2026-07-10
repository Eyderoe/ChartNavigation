#ifndef CHARTNAVIGATION_ENHANCEDTREE_HPP
#define CHARTNAVIGATION_ENHANCEDTREE_HPP


#include <QCoro/QCoroCore>
#include <unordered_set>

#include "services/settingManage.hpp"


class Node;
class Tree;

template <typename T>
concept NodeT = std::is_base_of_v<QTreeWidgetItem, T> || std::is_base_of_v<QTreeWidget, T>;

template <NodeT Parent>
int traverseRead (const QDir &folder, Parent *parentNode, int depth = 0);


class Node final : public QTreeWidgetItem {
    public:
        Node () = default;
        Node (QString baseDir, const QString &name, bool isFolder);
        uint64_t getHash ();
        void switchColor ();

        QString baseDir; // 文件或目录路径
        uint64_t hash{}; //  哈希值
        QColor color; // 节点颜色
        bool isFolder{}; // 类型
        bool isPdf{}; // PDF文件
        bool isRawColor{true}; // 是否显示本身颜色
};

class Tree final : public QTreeWidget {
    public:
        explicit Tree (QWidget *parent = nullptr);
        ~Tree () override;
        void loadFolder (const QString &folder);
    private:
        QDir cacheDir;
        std::unordered_set<Node*> visibleNodes; // 可视范围内的节点
        bool showThumbPic{false}, darkTheme{false}; // 显示缩略图
        bool shouldClean{false};

        QCoro::Task<> loadThumb (Node *item) const;
    private Q_SLOTS:
        void expand (const QTreeWidgetItem *item);
        void collapse (const QTreeWidgetItem *item);
};


/**
 * @brief 递归新建树节点
 * @tparam Parent 父节点类,根树或节点
 * @param folder 父文件夹
 * @param parentNode 父节点
 * @param depth 当前深度,最大深度4
 */
template <NodeT Parent>
int traverseRead (const QDir &folder, Parent *parentNode, const int depth) {
    static SettingsManager &ins = SettingsManager::instance();
    static const bool onlyPdf = ins.get(SettingsManager::onlyDisplayPdf, true).toBool();

    if (depth > 4)
        return 0;
    int pdfFileCount = 0;
    // 获取子文件夹/文件
    QStringList files = folder.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot
                                         , QDir::DirsFirst | QDir::Name);
    std::vector<Node*> nodes;
    for (auto &fileName : files) {
        QString child{folder.filePath(fileName)};
        if (QFileInfo info(child); info.isFile()) {
            const bool isPdf{info.suffix().toLower() == "pdf"};
            if (onlyPdf && (!isPdf))
                continue;
            if (isPdf)
                pdfFileCount++;
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
            pdfFileCount += traverseRead(node->baseDir, node, depth + 1);
    }
    return pdfFileCount;
}


#endif //CHARTNAVIGATION_ENHANCEDTREE_HPP
