#ifndef FP_HPP
#define FP_HPP

#include <unordered_set>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include "dataload/data_loader.hpp"

class FPTree {
public:

    struct VectorHash {
        std::size_t operator()(const std::vector<int>& vec) const {
            std::size_t hash = vec.size();  // 先加入size，避免不同长度的vector碰撞
            for (int val : vec) {
                // 使用加法组合，顺序无关（加法满足交换律）
                hash += std::hash<int>{}(val);
            }
            return hash;
        }
    };

    // 自定义相等性比较器（不依赖顺序）
    struct VectorEqual {
        bool operator()(const std::vector<int>& a, const std::vector<int>& b) const {
            if (a.size() != b.size()) return false;
            
            // 创建副本并排序后比较
            std::vector<int> sorted_a = a;
            std::vector<int> sorted_b = b;
            std::sort(sorted_a.begin(), sorted_a.end());
            std::sort(sorted_b.begin(), sorted_b.end());
            
            return sorted_a == sorted_b;
        }
    };

   

    struct FPNode {
        int item;                    // 项的值
        int count;                   // 支持计数
        std::unordered_map<int, FPNode*> children; // 子节点映射表 (item -> node)
    };
    FPTree(const DataLoader& db, double min_support);
    ~FPTree();


    using level = std::unordered_set<std::vector<int>, VectorHash, VectorEqual>;


    // 频繁项集结果
    std::vector<level> levels;
    /**
     * 构建FP-Tree并挖掘频繁项集
     */
    void buildFPTree();
    
    /**
     * 获取频繁1项集（用于条件FP-Tree构建）
     */
    std::vector<std::pair<int, const std::vector<int>* >> getFrequent1Itemsets();
    
    /**
     * 显示所有频繁项集
     */
    void displayFrequentItemsets();
    
    /**
     * 显示FP-Tree结构（按层展示）
     */
    void showTree();
    
    /**
     * 显示指定item的条件FP-Tree（miniFP-Tree）结构
     * @param item 要展示的item编号
     */
    void showMiniTree(int item);
    
    /**
     * 导出频繁项集结果（用于与Apriori比对）
     * 格式：Level X (频繁Y项集): 支持计数=xxx
     */
    void exportFrequentItemsets();
    
    /**
     * 获取所有频繁项集
     */
    const std::vector<level>& getFrequentItemsets() const {
        return levels;
    }

private:
    const DataLoader& db_;
    double min_support_;
    int min_support_count_;      // 最小支持计数（绝对数量）

    struct item_node{
        std::vector<int> stack;
        int count;
    };


    
    // FP-Tree结构
    FPNode* root_;                // 根节点
    
    // 步骤2: 构建FP-Tree（使用原始数据，按频繁项顺序构建）
    void buildTree(const std::vector<std::pair<int, const std::vector<int>*>> frequent_items);

    /**
     * 检查并挖掘item的频繁项集
     * @param item 当前项
     */
    void check();

    void get_cpb(FPNode* node, std::unordered_map<int, std::vector<item_node>>& cpbs, std::vector<int>& stack);

    /**
     * 构建miniFP-Tree
     * @param nodes 条件模式基节点列表
     * @param root 当前根节点
     * @return miniFP-Tree的追踪节点
     */
    void buildMiniTree(const std::vector<item_node>& nodes,FPNode* root);

    void destroyTree(FPNode* node);

    void dfs(std::vector<item_node>& node_cpd, std::vector<int>& stack);

    std::vector<std::pair<int, int>> cpb_count(const std::unordered_map<int, std::vector<item_node>>& cpbs);

    std::vector<std::pair<int,item_node>> load_inverted(const std::vector<item_node>& nodes);
    

};

#endif // FP_HPP
