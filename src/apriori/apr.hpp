#ifndef APR_HPP
#define APR_HPP

#include <cstddef>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <algorithm>
#include "dataload/data_loader.hpp"

// 为 vector<int> 提供哈希函数
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


using std::vector;
using std::unordered_map;
using std::mutex;
using std::unordered_set;

class Apriori {
public:

    struct node{
        vector<int> items;
        vector<int> records;
    };

    using Level = vector<node>;
    Apriori(const DataLoader& db, double confidence,int tnumber);
    ~Apriori();


    /**
     * 集合交集工具：计算两个集合的交集
     * @param set1 第一个集合
     * @param set2 第二个集合  
     * @return 两个集合的交集
     */
    vector<int> intersectSets(const vector<int>& set1, const vector<int>& set2);

    /**
     * 集合并集工具：计算两个集合的并集
     * @param set1 第一个集合
     * @param set2 第二个集合  
     * @return 两个集合的并集
     */
    vector<int> unionSets(const vector<int>& set1, const vector<int>& set2);
    
    /**
     * 显示level0的内容（单个元素的频繁项集）
     */
    void displayLevel0();
    
    /**
     * 显示所有level的内容
     */
    void displayAllLevels();
    
    /**
     * 构建完整的Apriori table
     * @param blocks 分块数量
     */
    void buildAprioriTable(size_t blocks);
    
    /**
     * 处理项集对组合
     * @param startPair 起始项集对索引
     * @param endPair 结束项集对索引
     * @param currentLevelMap 当前level的映射
     * @param currentLevel 当前level
     * @param levelResults 所有level的结果存储
     * @param levelMutexes 所有level的互斥锁
     */
     void processItemsetPairs(size_t startblock, size_t endblock,size_t block_size, int currentLevel, mutex& writeMutex, unordered_set<vector<int>, VectorHash, VectorEqual>& runtimeset);

     void displayLevel(int level);

private:
    //input vector
    DataLoader db;
    double confidence;
    int co;

    //置信度频繁数量
    size_t confidence_count;

    //aprior table
    vector<Level> lmap;
    vector<vector<int>> node_map;

    bool CheckInDB(vector<int> data);
    int CaculateBlocks(int co);
    
};

#endif // APR_HPP