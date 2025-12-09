#include "apr.hpp"
#include "dataload/data_loader.hpp"
#include "threadsignal.hpp"
#include <clocale>
#include <cmath>
#include <algorithm>
#include <cstddef>
#include <iostream>
#include <mutex>
#include <future>
#include <unordered_set>

using std::mutex;
using std::future;
const int hard_thread = std::thread::hardware_concurrency();
using std::abs;
using std::lock_guard;
using std::cout;
using std::endl;
using std::min;
using std::ceil;
using std::sqrt;
using std::unordered_set;

Apriori::Apriori(const DataLoader& db, double confidencel, int tnumber)
    : db(db), confidence(confidencel), co(tnumber)
{
    double support_count = confidence * db.all_count;
    confidence_count = static_cast<size_t>(std::ceil(support_count));
    if (confidence_count < 1) confidence_count = 1;

    // DataLoader 已经完成了倒排索引的转换，直接复制到 node_map
    // 获取倒排索引
    const auto& invertedIndex = db.getInvertedIndex();

    // 直接复制倒排索引到 node_map
    node_map = invertedIndex;

    // 初始化level0 - 单个元素的频繁项集（使用线程池并发处理）
    Level level0;
    mutex level0Mutex;

    // 获取线程池实例并确认并发数量
    auto& tpool = getThreadPool(co > 0 ? co: hard_thread);
    size_t totalElements = invertedIndex.size();

    level0.reserve(invertedIndex.size());

    for (int i=0;i<invertedIndex.size();i++){
        // 使用静态记录的支持计数进行比较，避免重复的除法运算
        if (invertedIndex[i].size() < confidence_count){
            continue;
        }

        node n;
        n.items = {i};
        n.records = invertedIndex[i];
        level0.push_back(n);
    }

    // 将level0添加到lmap
    lmap.push_back(level0);

    // 预分配一些空 level，避免后续越界
    lmap.resize(10);

    // 并发分片数量
    int blocks = CaculateBlocks(tnumber);

    // 构建完整的Apriori table
    buildAprioriTable(blocks);
}

Apriori::~Apriori() {
}

int Apriori::CaculateBlocks(int co) {
    // 我们需要找到最小的 n 使得 C(n, 2) >= co
    if (co <= 0) return 1;  // 最小返回1块

    // 通过解二次方程 n^2 - n - 2*co = 0 来估算 n
    int estimated_n = static_cast<int>(ceil((1 + sqrt(1 + 8.0 * co)) / 2));

    // 使用for循环查找最接近的n值
    int best_n = 2;  // 最小从2开始，因为C(2,2)=1
    int min_diff = abs(co - 1);  // C(2,2)=1与co的差值

    // 从2开始向上搜索，找到最接近co的组合数对应的n
    for (int n = 3; n <= estimated_n + 10; ++n) {
        int combinations = n * (n - 1) / 2;
        int diff = abs(combinations - co);

        if (diff < min_diff) {
            min_diff = diff;
            best_n = n;
        }

        // 如果已经超过co很多，可以提前终止
        if (combinations >= co && n > estimated_n) {
            break;
        }
    }

    return best_n;
}

void Apriori::buildAprioriTable(size_t blocks) {
    // 获取线程池实例
    auto& tpool = getThreadPool(co > 0 ? static_cast<size_t>(co) : 0);

    // currentLevel 表示当前正在构建的 level（从 level1 开始，即 2项集）
    int currentLevel = 1;

    while (currentLevel > 0 &&  !lmap[currentLevel-1].empty()) {
        cout << "构建Level " << currentLevel << "（" << (currentLevel+1) << "项集）..." << endl;
        cout << "从Level " << (currentLevel-1) << "（" << currentLevel << "项集）开始，包含 " << lmap[currentLevel-1].size() << " 个项集" << endl;

        const auto& currentLevelMap = lmap[currentLevel-1];
        size_t totalItemsets = currentLevelMap.size();

        // 存储所有任务的future
        vector<future<void>> futures;

        auto nums = min(static_cast<size_t>(blocks), currentLevelMap.size());
        auto blocksize = currentLevelMap.size() / nums;
        blocksize+=1;

        mutex writeMutex;

        auto& pool = getThreadPool(this->co > 0 ? this->co : hard_thread);

        // 初始化当前 level（清空之前的可能残留数据）
        if (currentLevel >= static_cast<int>(lmap.size())) {
            lmap.resize(currentLevel + 1);
        } else {
            lmap[currentLevel].clear();
        }

        auto runtimeset = unordered_set<vector<int>, VectorHash, VectorEqual>();


        // 处理所有块对：包括同一块内的组合（i==j）和不同块之间的组合（i<j）
        for(int i=0;i<(nums);i++){
            for(int j=i;j<(nums);j++){  // 改为 j=i，包含 i==j 的情况

                futures.push_back(pool.submit_task([this, i, j, blocksize, &runtimeset, currentLevel, &writeMutex]() {
                    processItemsetPairs(i, j, blocksize, currentLevel, writeMutex, runtimeset);
                }));
            }
        }

        for(auto& future : futures){
            future.wait();
        }

        cout << "Level " << currentLevel << " 构建完成，生成 " << lmap[currentLevel].size() << " 个项集" << endl;

        // 检查下一级是否有结果，如果没有就停止
        if (lmap[currentLevel].empty()) {
            break;
        }

        currentLevel+=1;  // 正确递增到下一级（每次+1，不是乘以2）

        // 安全检查，避免无限循环
        if (currentLevel > 100) {
            cout << "警告：达到最大 level 限制，停止迭代" << endl;
            break;
        }
    }
}

void Apriori::processItemsetPairs(size_t startblock, size_t endblock,size_t block_size, int currentLevel, mutex& writeMutex, unordered_set<vector<int>, VectorHash, VectorEqual>& runtimeset) {

    auto& level_map = lmap[currentLevel-1];

    auto local_stroage = vector<node>();
    local_stroage.reserve(block_size*(block_size-1)/2);

    auto first = level_map.begin();

    auto b1 = startblock*block_size;
    auto e1 = min(b1+block_size, level_map.size());
    auto b2 = endblock*block_size;
    auto e2 = min(b2+block_size, level_map.size());

    // 处理同一块内的组合（i==j）和不同块之间的组合（i<j）
    for (int i=b1;i<e1;i++){
        // 当处理同一块时（startblock == endblock），只处理 i < j 的情况，避免重复
        // 当处理不同块时，处理所有组合
        int j_start = (startblock == endblock) ? (i + 1) : b2;
        for (int j=j_start;j<e2;j++){
            auto& k1 = level_map[i].items;
            auto& k2 = level_map[j].items;
            auto key = unionSets(k1, k2);

            if(key.size() != currentLevel+1){
                continue;
            }

            auto& v1 = level_map[i].records;
            auto& v2 = level_map[j].records;
            auto value = intersectSets(v1, v2);

            // 使用静态记录的支持计数进行比较，避免重复的除法运算
            if (value.size() < confidence_count){
                continue;
            }

            local_stroage.push_back({key, value});
        }
    }

    lock_guard<mutex> lock(writeMutex);
    for(auto item : local_stroage){
        auto  get = runtimeset.find(item.items);
        if (get==runtimeset.end()){
            lmap[item.items.size()-1].push_back(item);
            runtimeset.insert(item.items);
        }

    }

}


vector<int> Apriori::unionSets(const vector<int>& vec1, const vector<int>& vec2) {
    vector<int> result;
    result.reserve(vec1.size() + vec2.size());

    size_t i = 0, j = 0;
    while (i < vec1.size() && j < vec2.size()) {
        if (vec1[i] < vec2[j]) {
            result.push_back(vec1[i]);  // 先 push，再移动索引
            i++;
        } else if (vec1[i] > vec2[j]) {
            result.push_back(vec2[j]);  // 先 push，再移动索引
            j++;
        } else {
            result.push_back(vec1[i]);
            i++;
            j++;
        }
    }

    while (i < vec1.size()) {
        result.push_back(vec1[i]);
        i++;
    }

    while (j < vec2.size()) {
        result.push_back(vec2[j]);
        j++;
    }

    return result;
}

vector<int> Apriori::intersectSets(const vector<int>& vec1, const vector<int>& vec2) {
    vector<int> result;

    // 使用双指针算法计算两个有序向量的交集
    size_t i = 0, j = 0;
    while (i < vec1.size() && j < vec2.size()) {
        if (vec1[i] < vec2[j]) {
            i++;
        } else if (vec1[i] > vec2[j]) {
            j++;
        } else {
            // 找到相同的元素
            result.push_back(vec1[i]);
            i++;
            j++;
        }
    }

    return result;
}

void Apriori::displayLevel0() {
    if (lmap.empty() || lmap[0].empty()) {
        std::cout << "Level0 is empty!" << std::endl;
        return;
    }

    std::cout << "Level0 (单个元素的频繁项集):" << std::endl;
    std::cout << "项集\t\t支持度" << std::endl;
    std::cout << "----------------------" << std::endl;

    for (const auto& [itemset, recordSet] : lmap[0]) {
        std::cout << "{";
        for (size_t i = 0; i < itemset.size(); ++i) {
            std::cout << itemset[i];
            if (i < itemset.size() - 1) std::cout << ", ";
        }
        std::cout << "}\t\t" << recordSet.size() << std::endl;
    }

    std::cout << "总计: " << lmap[0].size() << " 个频繁1项集" << std::endl;
}

void Apriori::displayAllLevels() {
    if (lmap.empty()) {
        std::cout << "Apriori table is empty!" << std::endl;
        return;
    }

    std::cout << "\n========== Apriori 算法完整结果 ==========" << std::endl;


    size_t totalFrequentItemsets = 0;

    size_t lc=0;

    for (size_t level =0; level < lmap.size(); ++level) {
        if (lmap[level].empty()) {
            continue;
        }
        lc++;

        std::cout << "\nLevel " << level << " (频繁" << (level + 1) << "项集):" << std::endl;

        size_t levelcount = lmap[level].size();

        std::cout << "总计: " << levelcount << " 个频繁" << (level + 1) << "项集" << std::endl;


        totalFrequentItemsets += levelcount;
    }

    std::cout << "\n==========================================" << std::endl;
    std::cout << "所有级别总计: " << totalFrequentItemsets << " 个频繁项集" << std::endl;
    std::cout << "最大级别: " <<lc<< std::endl;
    std::cout << "置信度阈值: " << confidence << std::endl;
    std::cout << "==========================================" << std::endl;
}

void Apriori::displayLevel(int level) {
    if (lmap.empty() || lmap[level].empty()) {
        std::cout << "Level " << level << " is empty!" << std::endl;
        return;
    }

    std::cout << "Level " << level << " (频繁" << (level + 1) << "项集):" << std::endl;
    std::cout << "项集\t\t支持度" << std::endl;
    std::cout << "----------------------" << std::endl;

    for (const auto& [itemset, recordSet] : lmap[level]) {
        std::cout << "{";
        for (size_t i = 0; i < itemset.size(); ++i) {
            std::cout << itemset[i];
            if (i < itemset.size() - 1) std::cout << ", ";
        }
        std::cout << "}\t\t" << recordSet.size() << std::endl;
    }

    std::cout << "总计: " << lmap[level].size() << " 个频繁" << (level + 1) << "项集" << std::endl;
}
