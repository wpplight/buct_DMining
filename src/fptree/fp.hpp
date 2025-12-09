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
        int item;                    // 项的值，-1表示根节点
        int count;                   // 支持计数
        std::unordered_map<int, FPNode*> children; // 子节点映射表 (item -> node)
    };
    
    FPTree(const DataLoader& db, double min_support);
    ~FPTree();

    using level = std::unordered_set<std::vector<int>, VectorHash, VectorEqual>;

    // 频繁项集结果
    std::vector<level> levels;
    
    /**
     * 获取频繁1项集（用于条件FP-Tree构建）
     */
    std::vector<std::pair<int, const std::vector<int>*>> getFrequent1Itemsets();
    
    /**
     * 显示FP-Tree结构（按层展示）
     */
    void showTree();
    
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
        std::vector<int> stack;   // 路径
        int count;               // 支持计数
    };

    // FP-Tree结构
    FPNode* root_;                // 根节点
    
    // 条件模式基：{item: [(path, node), ...]}
    // path是从根到父节点的路径，node是新创建的节点（用于获取动态更新的count）
    std::unordered_map<int, std::vector<std::pair<std::vector<int>, FPNode*>>> conditional_pattern_bases_;
    
    /**
     * 构建FP-Tree（使用原始数据，按频繁项顺序构建）
     */
    void buildTree(const std::vector<std::pair<int, const std::vector<int>*>> frequent_items);
    
    /**
     * 挖掘频繁项集（从支持度最低的项开始，自底向上递归）
     */
    void check(const std::vector<std::pair<int, const std::vector<int>*>>& frequent_items);
    
    /**
     * 递归挖掘频繁项集
     * @param stack 当前项集栈
     * @param conditional_patterns 条件模式基
     */
    void dfs(std::vector<int>& stack, const std::vector<item_node>& conditional_patterns);
    
    /**
     * 从条件模式基中生成新的条件模式基（提取包含target_item的前缀路径）
     */
    std::vector<item_node> generateNewPatterns(
        const std::vector<item_node>& patterns, 
        int target_item);
    

    /**
     * 销毁FP-Tree
     */
    void destroyTree(FPNode* node);
};

#endif // FP_HPP
