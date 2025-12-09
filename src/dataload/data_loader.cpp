#include "data_loader.hpp"
#include "threadsignal.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <thread>
#include <utility>
#include <vector>
#include <string>
#include <mutex>
#include <future>
#include <algorithm>

using std::vector;
using std::string;
using std::cout;
using std::endl;

using std::ifstream;
using std::stringstream;
using std::mutex;
using std::future;
using std::lock_guard;

//硬件限制数量
const int  hard_thread = std::thread::hardware_concurrency();

DataLoader::DataLoader(const std::string& filename, char delimiter, int thread_count) 
    : max_record_size_(0), max_num_of_record(0), record_count_(0) {
    
    cout << "正在加载csv数据文件到内存: " << filename << "..." << endl;
    
    // 第一阶段：串行读取所有行到内存
    vector<string> rawLines=baseLoad(filename);
    
    cout << "文件读取完成，共 " << rawLines.size() << " 行数据" << endl;
    
    // 预分配内存
    records_.resize(rawLines.size());
    
    // 第二阶段：并发解析数据
    parseLinesConcurrently(rawLines, delimiter, thread_count);
    
    
    record_count_ = records_.size();
    cout << "数据解析完成！共解析 " << record_count_ 
              << " 条记录，最大记录长度: " << max_record_size_ 
              << "，最大元素值: " << max_num_of_record << endl;
    
    // 转换为倒排索引
    cout << "正在转换为倒排索引..." << endl;
    convertToInvertedIndex(thread_count);
    
    // 保留原始数据，供FP-Tree等算法使用
    cout << "倒排索引转换完成，原始数据已保留！" << endl;
}

DataLoader::~DataLoader() {
    // 析构函数：自动释放所有数据
}

std::vector<std::string> DataLoader::readAllLines(const std::string& filename) {
    vector<string> rawLines;
    ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("无法打开文件: " + filename);
    }
    
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            rawLines.push_back(line);
        }
    }
    file.close();
    
    return rawLines;
}

void DataLoader::parseLinesConcurrently(const vector<string>& rawLines, char delimiter, int thread_count) {
    size_t totalLines = rawLines.size();

    size_t numThreads = thread_count > 0 ? thread_count: hard_thread;
    
    if (numThreads > totalLines) numThreads = totalLines;
    
    // 计算每个线程处理的行数
    size_t linesPerThread = totalLines / numThreads;
    if (linesPerThread == 0) {
        linesPerThread = 1;
    }
    
    // 获取线程池实例
    auto& tpool = getThreadPool(numThreads);
        
    // 存储线程局部统计信息
    vector<size_t> threadMaxRecordSizes(numThreads, 0);
    vector<int> threadMaxNums(numThreads, 0);
    vector<future<void>> futures;
    
    // 将数据分割并提交任务到线程池
    for (size_t t = 0; t < numThreads; t++) {
        size_t startIdx = t * linesPerThread;
        size_t endIdx = (t == numThreads - 1) ? totalLines : (t + 1) * linesPerThread;
        
        futures.push_back(tpool.submit_task([this, &rawLines, startIdx, endIdx, t, 
                            &threadMaxRecordSizes, &threadMaxNums, delimiter]() {
            parseLinesRange(rawLines, startIdx, endIdx, delimiter, 
                          threadMaxRecordSizes[t], threadMaxNums[t]);
        }));
    }
    
    // 等待所有任务完成
    for (auto& future : futures) {
        future.wait();
    }
    
    // 合并统计信息
    mergeThreadStats(threadMaxRecordSizes, threadMaxNums);
}

void DataLoader::parseLinesRange(const vector<string>& rawLines, 
                                size_t startIdx, size_t endIdx, char delimiter,
                                size_t& localMaxRecordSize, int& localMaxNum) {
    for (size_t lineIdx = startIdx; lineIdx < endIdx; lineIdx++) {
        const string& line = rawLines[lineIdx];
        Record record = parseLine(line, delimiter);
        
        // 更新本地最大数字
        for (int num : record) {
            if (num > localMaxNum) {
                localMaxNum = num;
            }
        }
        
        // 保存记录（如果记录不为空）
        if (!record.empty()) {
            size_t record_size = record.size();
            records_[lineIdx] = std::move(record);
            
            // 更新本地最大记录长度
            if (record_size > localMaxRecordSize) {
                localMaxRecordSize = record_size;
            }
        }
    }
}

DataLoader::Record DataLoader::parseLine(const string& line, char delimiter) {
    Record record;
    stringstream ss(line);
    string field;
    
    while (getline(ss, field, delimiter)) {
        // 去除前后空白
        size_t start = field.find_first_not_of(" \t\n\r");
        if (start != string::npos) {
            size_t end = field.find_last_not_of(" \t\n\r");
            field = field.substr(start, end - start + 1);
        } else {
            field.clear();
        }
            
        if (!field.empty()){
                int num = stoi(field);
                record.push_back(num);
        }
    }
    
    return record;
}

void DataLoader::mergeThreadStats(const vector<size_t>& threadMaxRecordSizes, 
                                 const vector<int>& threadMaxNums) {
    for (size_t t = 0; t < threadMaxRecordSizes.size(); t++) {
        if (threadMaxRecordSizes[t] > max_record_size_) {
            max_record_size_ = threadMaxRecordSizes[t];
        }
        if (threadMaxNums[t] > max_num_of_record) {
            max_num_of_record = threadMaxNums[t];
        }
    }
}
// 元素从1开始编号，所以需要+1
void DataLoader::initializeInvertedIndex() {
    int indexSize = max_num_of_record + 1;  
    inverted_index_.resize(indexSize);
    for (int i = 0; i < indexSize; i++) {
        inverted_index_[i] = vector<int>();
    }
}

void DataLoader::buildInvertedIndexConcurrently(size_t thread_count) {
    size_t totalRecords = records_.size();
    size_t numThreads = thread_count > 0 ? thread_count : hard_thread ;
    if (numThreads == 0) numThreads = 1;
    if (numThreads > totalRecords) numThreads = totalRecords;
    
    // 计算每个线程处理的记录数
    size_t recordsPerThread = totalRecords / numThreads;
    if (recordsPerThread == 0) {
        recordsPerThread = 1;
    }
    
    // 获取线程池实例
    auto& tpool = getThreadPool(thread_count > 0 ? static_cast<size_t>(thread_count) : 0);
    
    // 用于保护倒排索引写入的互斥锁
    mutex indexMutex;
    vector<future<void>> futures;
    
    // 将数据分割并提交任务到线程池
    for (size_t t = 0; t < numThreads; t++) {
        size_t startIdx = t * recordsPerThread;
        size_t endIdx = (t == numThreads - 1) ? totalRecords : (t + 1) * recordsPerThread;
        
        futures.push_back(tpool.submit_task([this, startIdx, endIdx, &indexMutex]() {
            buildInvertedIndexRange(startIdx, endIdx, indexMutex);
        }));
    }
    
    // 等待所有任务完成
    for (auto& future : futures) {
        future.wait();
    }
}

void DataLoader::buildInvertedIndexRange(size_t startIdx, size_t endIdx, mutex& indexMutex) {
    // 为当前线程创建本地临时存储，减少锁竞争
    vector<vector<int>> localResults(inverted_index_.size());
    
    // 处理分配给当前线程的记录范围，先收集到本地
    for (size_t recordIdx = startIdx; recordIdx < endIdx; recordIdx++) {
        const auto& record = records_[recordIdx];
        
        // 遍历当前记录中的所有元素
        for (int element : record) {
            // 确保元素在有效范围内（允许element为0）
            if (element >= 0 && element < static_cast<int>(inverted_index_.size())) {
                // 先写入本地临时存储，无需加锁
                localResults[element].push_back(recordIdx);
            }
        }
    }
    
    // 对每个线程内部的倒排索引进行排序，确保有序
    for (size_t element = 0; element < localResults.size(); element++) {
        if (!localResults[element].empty()) {
            std::sort(localResults[element].begin(), localResults[element].end());
        }
    }
    
    // 批量写入到全局倒排索引，减少锁竞争
    lock_guard<mutex> lock(indexMutex);
    for (size_t element = 0; element < localResults.size(); element++) {
        if (!localResults[element].empty()) {
            // 将本地结果追加到全局索引（本地结果已排序，合并后需要再次排序）
            inverted_index_[element].insert(
                inverted_index_[element].end(),
                localResults[element].begin(),
                localResults[element].end()
            );
        }
    }
}

void DataLoader::sortInvertedIndex() {
    // 对倒排索引中每个元素的记录索引列表进行排序
    for (auto& recordList : inverted_index_) {
        if (!recordList.empty()) {
            std::sort(recordList.begin(), recordList.end());
        }
    }
}

void DataLoader::convertToInvertedIndex(int thread_count) {
    // 初始化倒排索引大小
    initializeInvertedIndex();
    
    // 如果记录数为0，直接返回
    if (records_.empty()) {
        return;
    }
    
    buildInvertedIndexConcurrently(thread_count);
    
    // 对所有倒排索引中的记录列表进行排序，确保有序
    sortInvertedIndex();
}

vector<string> DataLoader::baseLoad(string file_name) {
    ifstream file(file_name);
    string line;
    vector<string> lines;
    
    while (getline(file, line)) {
        lines.push_back(line);
    }

    all_count = lines.size();
    
    file.close();
    
    //避免析构函数处罚：）
    return std::move(lines);
}