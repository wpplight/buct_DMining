#include "fp.hpp"
#include <cstddef>
#include <iostream>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <utility>
#include <vector>

using std::cout;
using std::endl;
using std::vector;
using std::pair;
using std::unordered_map;
using std::unordered_set;
using std::sort;

FPTree::FPTree(const DataLoader& db, double min_support)
    : db_(db), min_support_(min_support), root_(nullptr), min_support_count_(0) {
    // 计算最小支持计数（绝对数量）
    min_support_count_ = static_cast<int>(min_support * db_.all_count);
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
    
    // 步骤2: 构建FP-Tree（直接使用原始数据，按排序后的频繁项顺序构建）
    cout << "\n步骤2: 构建FP-Tree..." << endl;
    buildTree(frequent_items);
    
    
    // 步骤3: 使用索引系统挖掘频繁项集
    cout << "\n步骤3: 挖掘频繁项集..." << endl;
    
    // 首先记录所有频繁1项集
    if (levels.empty()) {
        levels.resize(10);  // 预分配空间，避免越界
    }
    for(const auto& [item, count] : frequent_items){
        levels[0].insert({item});
    }

    check();

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


vector<pair<int,const std::vector<int>* >> FPTree::getFrequent1Itemsets() {
    
    const auto& inverted_index = db_.getInvertedIndex();
    
    vector<pair<int,const std::vector<int>* >> frequent_items;
    for(int i=0;i<inverted_index.size();i++){
        if(inverted_index[i].size() < min_support_count_){
            continue;
        }

        frequent_items.emplace_back(i, &inverted_index[i]);
    }

    sort(frequent_items.begin(), frequent_items.end(),
    [](const pair<int,const std::vector<int>*>& a, const pair<int,const std::vector<int>*>& b) {
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

    for(const auto& item : frequent_items){
        auto index = item.first;
        auto& records = *item.second;

        // 用于追踪已记录的节点，避免重复记录条件模式基
        auto recorded_nodes = std::unordered_set<FPNode*>();

        for(const auto& record : records){
            auto& node = nodes[record];
            
            auto get = node->children.find(index);

            if(get == node->children.end()){
                auto new_node = new FPNode{index, 1};
                node->children[index] = new_node;
                nodes[record] = new_node;
                
                // 记录条件模式基：记录新创建的节点（用于后续提取从根到父节点的路径）
                // 使用节点指针作为key来避免重复记录相同的节点
                if(recorded_nodes.find(new_node) == recorded_nodes.end()){
                    recorded_nodes.insert(new_node);
                }
                continue;
            }

            get->second->count++;
            nodes[record] = get->second;
        }
    }

    auto endtime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endtime - begintime);
    cout << "FP-Tree构建完成，耗时: " << duration.count() << "ms" << endl;
}


//用于递归建树挖掘
void FPTree::buildMiniTree(const std::vector<item_node>& nodes,FPNode* root) {
   
    auto array_inverted = load_inverted(nodes);

    //状态追踪
    auto status_map = unordered_map<int, FPTree::FPNode*>();

    //init
    auto len = nodes.size();
    for(size_t i=0;i<len;i++){
        status_map[i] = root;
    }

    //tree build
    cout << "[buildMiniTree] 开始构建树，array_inverted大小: " << array_inverted.size() << endl;
    for(const auto& [item, path] : array_inverted){
        for(int i : path.stack){            
            auto parent = status_map[i];
            
            if(parent->children.find(item) == parent->children.end()){
                auto new_node = new FPNode{item, path.count};
                parent->children[item] = new_node;
                status_map[i] = new_node;
            }
            else{
                parent->children[item]->count += path.count;
                status_map[i] = parent->children[item];
            }
        }
    }
}

void FPTree::check() {
    auto frequent_items = getFrequent1Itemsets();

    // 从主FP-Tree提取条件模式基
    auto cpbs = std::unordered_map<int, std::vector<item_node>>();
    auto stack = std::vector<int>();
    get_cpb(root_, cpbs, stack);

    if(cpbs.empty()) {
        cout << "[check] cpbs为空，返回" << endl;
        return;
    }

    cout << "[check] 提取到 " << cpbs.size() << " 个item的条件模式基" << endl;

    auto len = frequent_items.size();

    // 从支持度最低的项开始（自底向上）递归挖掘
    for(int i = static_cast<int>(len) - 1; i >= 0; i--){
        int item = frequent_items[i].first;
        
        // 检查该item是否有条件模式基
        auto it = cpbs.find(item);
        if(it == cpbs.end() || it->second.empty()){
            continue;
        }

        cout << "[check] 处理item: " << item << ", 条件模式基大小: " << it->second.size() << endl;
        
        auto stack = std::vector<int>();
        stack.push_back(item);
        
        // 递归挖掘
        dfs(it->second, stack);
    }
}

void FPTree::dfs(std::vector<item_node>& node_cpd, std::vector<int>& stack) {

    auto my_root = new FPNode{-1, 0};

    cout << "[dfs] 开始构建miniFP-Tree，node_cpd大小: " << node_cpd.size() << endl;
    buildMiniTree(node_cpd, my_root);
    cout << "[dfs] miniFP-Tree构建完成" << endl;

    auto this_cpb = unordered_map<int, std::vector<item_node>>();
    auto temp_stack = std::vector<int>();
    get_cpb(my_root, this_cpb, temp_stack);

    // 如果条件模式基为空，销毁树并返回
    if(this_cpb.empty()){
        cout << "[dfs] this_cpb为空，销毁树并返回" << endl;
        destroyTree(my_root);
        return;
    }

    auto frequent_items = cpb_count(this_cpb);
   
    cout << "[dfs] 找到 " << frequent_items.size() << " 个频繁项" << endl;

    // 对每个频繁项进行递归挖掘
    for(const auto& [item, count] : frequent_items){

        stack.push_back(item);
        levels[stack.size()-1].insert(stack);

        dfs(this_cpb[item], stack);

        stack.pop_back();
    }

    destroyTree(my_root);
}

vector<pair<int,FPTree::item_node>> FPTree::load_inverted(const std::vector<item_node>& nodes) {
    auto inverted_index = unordered_map<int, item_node>();
     //创建倒排索引 load
     for(size_t i=0;i<nodes.size();i++){
        auto& path = nodes[i].stack;
        for(int item : path){
            // 确保item_node被正确初始化
            if(inverted_index.find(item) == inverted_index.end()){
                inverted_index[item] = item_node{std::vector<int>{}, 0};
            }
            inverted_index[item].count += nodes[i].count;
            inverted_index[item].stack.push_back(static_cast<int>(i));
        }
    }

    auto array_inverted = vector<pair<int,item_node>>();

    //transform to array 使用move避免无效拷贝
    for(auto& [item, path] : inverted_index){
        if(path.count >= min_support_count_){
            array_inverted.push_back({item, path});
        }
    }

    //sort by count
    sort(array_inverted.begin(), array_inverted.end(),
    [](const pair<int,item_node>& a, const pair<int,item_node>& b) {
        return a.second.count < b.second.count;
    });

    return array_inverted;
}

vector<pair<int, int>> FPTree::cpb_count(const std::unordered_map<int, std::vector<item_node>>& cpbs) {

     // 统计每个项的支持度，筛选频繁项
    // 注意：统计的是path中的所有项，每个pattern的count都要累加
    unordered_map<int, int> item_counts;
    for(const auto& [item, patterns] : cpbs){
        for(const auto& pattern : patterns){
            // 统计路径中的所有项
            for(int path_item : pattern.stack){
                item_counts[path_item] += pattern.count;
            }
        }
    }

    // 筛选频繁项并按支持度降序排序
    vector<pair<int, int>> frequent_items;
    for(const auto& [item, count] : item_counts){
        if(count >= min_support_count_){
            frequent_items.push_back({item, count});
        }
    }

    sort(frequent_items.begin(), frequent_items.end(),
         [](const pair<int, int>& a, const pair<int, int>& b) {
             return a.second > b.second;
         });

    return frequent_items;
}

void FPTree::get_cpb(FPNode* node, std::unordered_map<int, std::vector<item_node>>& cpbs, std::vector<int>& stack) {
    if(node == nullptr) return;

    for(auto& [item, child] : node->children){
        if(stack.size()>0) cpbs[item].push_back({stack, child->count});
        stack.push_back(item);
        get_cpb(child, cpbs, stack);
        stack.pop_back();
    }
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
