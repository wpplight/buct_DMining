#include "fp.hpp"
#include <cstddef>
#include <iostream>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <utility>
#include <vector>
#include <chrono>
#include <cmath>

using std::cout;
using std::endl;
using std::vector;
using std::pair;
using std::unordered_map;
using std::unordered_set;
using std::sort;

FPTree::FPTree(const DataLoader& db, double min_support)
    : db_(db), min_support_(min_support), root_(nullptr), min_support_count_(0) {
  
    double support_count = min_support * db_.all_count;
    min_support_count_ = static_cast<int>(std::ceil(support_count));
    if (min_support_count_ < 1) min_support_count_ = 1;

    levels.reserve(10);

    cout << "\n========== FP-Tree 算法 ==========" << endl;
    cout << "最小支持度: " << min_support_ << " (最小支持计数: " << min_support_count_ << ")" << endl;
    
    // 步骤1: 计算频繁1项集并排序
    cout << "\n步骤1: 计算频繁1项集并排序..." << endl;
    auto frequent_items = getFrequent1Itemsets();
    cout << "找到 " << frequent_items.size() << " 个频繁1项集" << endl;

    if (frequent_items.empty()) {
        cout << "没有频繁项集，算法结束" << endl;
        return;
    }
    
    // 步骤2: 构建FP-Tree
    cout << "\n步骤2: 构建FP-Tree..." << endl;
    buildTree(frequent_items);
    
    // 步骤3: 挖掘频繁项集
    cout << "\n步骤3: 挖掘频繁项集..." << endl;
    
    // 初始化levels，记录频繁1项集
    levels.resize(10);  // 预分配空间
    for(const auto& item : frequent_items){
        levels[0].insert({item.first});
    }

    // 从支持度最低的项开始，递归挖掘频繁项集
    check(frequent_items);

    // 统计总数量
    size_t total_count = 0;
    for (const auto& level : levels) {
        total_count += level.size();
    }
    cout << "\nFP-Tree算法完成！共找到 " << total_count << " 个频繁项集" << endl;
}

FPTree::~FPTree() {
    if (root_) {
        destroyTree(root_);
    }
}

vector<pair<int, const std::vector<int>*>> FPTree::getFrequent1Itemsets() {
    const auto& inverted_index = db_.getInvertedIndex();
    
    vector<pair<int, const std::vector<int>*>> frequent_items;
    for(size_t i = 0; i < inverted_index.size(); i++){
        if(inverted_index[i].size() < static_cast<size_t>(min_support_count_)){
            continue;
        }
        frequent_items.emplace_back(static_cast<int>(i), &inverted_index[i]);
    }

    // 按支持度降序排序
    sort(frequent_items.begin(), frequent_items.end(),
    [](const pair<int, const std::vector<int>*>& a, const pair<int, const std::vector<int>*>& b) {
        return a.second->size() > b.second->size();
    });

    return frequent_items;
}

void FPTree::buildTree(const vector<pair<int, const std::vector<int>*>> frequent_items) {
   
    auto begintime = std::chrono::high_resolution_clock::now();

    // 创建根节点
    root_ = new FPNode{-1, 0};

    //原始数据
    auto len = db_.getOriginalData().size();

    //动态指针追踪
    vector<FPNode*> nodes(len, root_);

    vector<vector<int>> paths(len);

    for(const auto& item : frequent_items){
        auto index = item.first;
        auto& records = *item.second;

        for(const auto& record : records){
            auto& node = nodes[record];
            
            auto get = node->children.find(index);

            if(get == node->children.end()){
                auto new_node = new FPNode{index, 1};
                node->children[index] = new_node;
                nodes[record] = new_node;
                
                // 记录条件模式基：从根到父节点的路径
                if(!paths[record].empty()){
                    conditional_pattern_bases_[index].push_back({paths[record], new_node});
                }
                // 更新路径
                paths[record].push_back(index);
            } else {
                // 节点已存在，增加计数
                get->second->count++;
                nodes[record] = get->second;
                // 更新路径（即使节点已存在，路径也需要更新）
                paths[record].push_back(index);
            }
        }
    }
    auto endtime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endtime - begintime);
    cout << "FP-Tree构建完成，耗时: " << duration.count() << "ms" << endl;
}

void FPTree::check(const vector<pair<int, const std::vector<int>*>>& frequent_items) {
   auto item_map =unordered_map<int, vector<item_node>>();
   for(const auto& [item, patterns] : conditional_pattern_bases_){
        for(const auto& [path, node] : patterns){
            item_map[item].push_back({path, node->count});
        }
   }

   for(const auto& [item, patterns] : item_map){
       auto temp_stack = vector<int>();
       
       temp_stack.push_back(item);
       
       dfs(temp_stack,patterns);
    
   }
}

void FPTree::dfs(vector<int>& stack, const vector<item_node>& conditional_patterns) {
    
    // 统计条件模式基中每个项的支持度
    // 注意：统计的是path中的所有项，每个pattern的count都要累加
    unordered_map<int, int> item_counts;
    for(const auto& pattern : conditional_patterns){
        for(int item : pattern.stack){
            item_counts[item] += pattern.count;
        }
    }
    
    // 筛选频繁项
    vector<pair<int, int>> frequent_items;
    for(const auto& [item, count] : item_counts){
        if(count >= min_support_count_){
            frequent_items.push_back({item, count});
        }
    }
    
    if(frequent_items.empty()){
        return;
    }
    
    // 按支持度降序排序
    sort(frequent_items.begin(), frequent_items.end(),
         [](const pair<int, int>& a, const pair<int, int>& b) {
             return a.second > b.second;
         });
    
    // 对每个频繁项，生成新的频繁项集并递归挖掘
    for(const auto& [item, count] : frequent_items){
        // 添加当前项到栈中
        stack.push_back(item);
        

        // 确保levels有足够的空间
        size_t level_index = stack.size() - 1;
        if(level_index >= levels.size()){
            levels.resize(level_index + 1);
        }
        
        // 记录频繁项集
        levels[level_index].insert(stack);
        
        // 生成新的条件模式基
        vector<item_node> new_patterns = generateNewPatterns(conditional_patterns, item);
        
        // 递归挖掘
        dfs(stack, new_patterns);
        
        stack.pop_back();
    }
}

vector<FPTree::item_node> FPTree::generateNewPatterns(
    const vector<item_node>& patterns, 
    int target_item) {
    vector<item_node> new_patterns;
    
    for(const auto& pattern : patterns){
        // 找到target_item在path中的位置
        auto& path = pattern.stack;
        auto pos = std::find(path.begin(), path.end(), target_item);
        if(pos != path.end()){
            // 提取target_item之前的前缀路径，保留原来的count值
            vector<int> prefix(path.begin(), pos);
            // 即使prefix为空，也要保留（用于后续处理）
            new_patterns.push_back({prefix, pattern.count});
        }
    }
    
    return new_patterns;
}



void FPTree::destroyTree(FPNode* node) {
    if (node == nullptr) return;
    
    for (auto& [item, child] : node->children) {
        destroyTree(child);
    }
    
    delete node;
}

void FPTree::showTree() {
    if (root_ == nullptr) {
        cout << "FP-Tree 为空" << endl;
        return;
    }
    
    cout << "\n========== FP-Tree 结构展示 ==========" << endl;
    
    std::queue<pair<FPNode*, int>> q; // (node, level)
    q.push({root_, 0});
    
    int current_level = -1;
    
    while (!q.empty()) {
        auto [node, level] = q.front();
        q.pop();
        
        // 如果是新的一层，打印层标题
        if (level != current_level) {
            if (current_level != -1) {
                cout << endl;
            }
            current_level = level;
            if (level == 0) {
                cout << "Level " << level << " (根节点): ";
            } else {
                cout << "Level " << level << ": ";
            }
        }
        
        // 打印节点信息
        if (node->item == -1) {
            cout << "[ROOT]";
        } else {
            cout << "[" << node->item << ":" << node->count << "]";
        }
        
        // 将子节点加入队列
        for (auto& [item, child] : node->children) {
            q.push({child, level + 1});
        }
        
        // 如果这一层还有节点，添加分隔符
        if (!q.empty() && q.front().second == level) {
            cout << "  ";
        }
    }
    
    cout << "\n\n==========================================" << endl;
}
