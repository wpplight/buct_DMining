#ifndef DATA_LOADER_HPP
#define DATA_LOADER_HPP

#include <cstddef>
#include <vector>
#include <string>
#include <mutex>

/**
 * 数据加载器类
 * 负责加载和存储 CSV 数据，提供便捷的访问接口
 * 数据结构：倒排索引，元素值 -> 包含该元素的记录索引列表
 */
class DataLoader {
public:
    // 记录类型：每条记录是一个整数向量（仅在转换前使用）
    using Record = std::vector<int>;
    
    // 数据库类型：所有记录的集合（二维vector，仅在转换前使用）
    using Database = std::vector<Record>;
    
    // 倒排索引类型：元素值 -> 包含该元素的记录索引列表
    using InvertedIndex = std::vector<std::vector<int>>;

    size_t all_count=0;
    
    /**
     * 构造函数：加载指定文件的数据并转换为倒排索引
     * @param filename CSV 文件路径
     * @param delimiter 分隔符，默认为空格
     * @param thread_count 并发转换的线程数，默认为0（使用默认线程数）
     */
    DataLoader(const std::string& filename, char delimiter = ' ', int thread_count = 0);
    
    /**
     * 析构函数
     */
    ~DataLoader();
    /**
     * 获取记录总数
     * @return 记录数量
     */
    size_t getRecordCount() const noexcept {
        return record_count_;
    }

    int getMaxValue() const noexcept{
        return max_num_of_record;
    }
    
    /**
     * 获取记录总数（转换前的记录数）
     */
    size_t size() const noexcept {
        return record_count_;
    }
    
    /**
     * 通过元素值获取包含该元素的所有记录索引
     * @param element 元素值
     * @return 包含该元素的记录索引列表的常量引用
     */
    const std::vector<int>& getRecordsByElement(int element) const {
        if (element < 0 || element >= static_cast<int>(inverted_index_.size())) {
            static const std::vector<int> empty;
            return empty;
        }
        return inverted_index_[element];
    }
    
    /**
     * 通过元素值获取包含该元素的所有记录索引（重载[]操作符）
     * @param element 元素值
     * @return 包含该元素的记录索引列表的常量引用
     */
    const std::vector<int>& operator[](int element) const {
        return getRecordsByElement(element);
    }
    
    /**
     * 获取倒排索引的引用
     * @return 倒排索引的常量引用
     */
    const InvertedIndex& getInvertedIndex() const noexcept {
        return inverted_index_;
    }
    
    /**
     * 检查元素是否存在
     * @param element 元素值
     * @return 如果元素存在且至少出现在一条记录中返回true
     */
    bool hasElement(int element) const {
        if (element < 0 || element >= static_cast<int>(inverted_index_.size())) {
            return false;
        }
        return !inverted_index_[element].empty();
    }
    
    /**
     * 获取元素出现的记录数量（支持度）
     * @param element 元素值
     * @return 包含该元素的记录数量
     */
    size_t getElementSupport(int element) const {
        if (element < 0 || element >= static_cast<int>(inverted_index_.size())) {
            return 0;
        }
        return inverted_index_[element].size();
    }
    
    /**
     * 获取原始数据（所有记录的集合）
     * @return 原始数据的常量引用
     */
    const Database& getOriginalData() const noexcept {
        return records_;
    }
    
    /**
     * 获取指定索引的记录
     * @param index 记录索引
     * @return 记录的常量引用
     */
    const Record& getRecord(size_t index) const {
        if (index >= records_.size()) {
            static const Record empty;
            return empty;
        }
        return records_[index];
    }

private:
    /**
     * 读取文件所有行到内存
     * @param filename 文件名
     * @return 所有行的字符串向量
     */
    std::vector<std::string> readAllLines(const std::string& filename);
    
    /**
     * 并发解析数据行
     * @param rawLines 原始行数据
     * @param delimiter 分隔符
     * @param thread_count 线程数
     */
    void parseLinesConcurrently(const std::vector<std::string>& rawLines, char delimiter, int thread_count);
    
    /**
     * 解析指定范围内的数据行
     * @param rawLines 原始行数据
     * @param startIdx 起始索引
     * @param endIdx 结束索引
     * @param delimiter 分隔符
     * @param localMaxRecordSize 本地最大记录长度
     * @param localMaxNum 本地最大数字
     */
    void parseLinesRange(const std::vector<std::string>& rawLines, 
                        size_t startIdx, size_t endIdx, char delimiter,
                        size_t& localMaxRecordSize, int& localMaxNum);
    
    /**
     * 解析单行数据
     * @param line 单行字符串
     * @param delimiter 分隔符
     * @return 解析后的记录
     */
    Record parseLine(const std::string& line, char delimiter);
    
    /**
     * 合并线程统计信息
     * @param threadMaxRecordSizes 线程最大记录长度
     * @param threadMaxNums 线程最大数字
     */
    void mergeThreadStats(const std::vector<size_t>& threadMaxRecordSizes, 
                         const std::vector<int>& threadMaxNums);
    
    /**
     * 初始化倒排索引
     */
    void initializeInvertedIndex();
    
    /**
     * 并发构建倒排索引
     * @param thread_count 线程数
     */
    void buildInvertedIndexConcurrently(size_t thread_count);
    
    /**
     * 构建指定范围内的倒排索引
     * @param startIdx 起始索引
     * @param endIdx 结束索引
     * @param indexMutex 索引互斥锁
     */
    void buildInvertedIndexRange(size_t startIdx, size_t endIdx, std::mutex& indexMutex);
    
    /**
     * 将原始数据转换为倒排索引（并发版本）
     * @param thread_count 线程数
     */
    void convertToInvertedIndex(int thread_count);
    
    /**
     * 对倒排索引中每个元素的记录索引列表进行排序
     */
    void sortInvertedIndex();
    
    /**
     * 将磁盘数据转化为字符串数组
     * @param file_name 文件名
     * @return 字符串数组
     */
     std::vector<std::string> baseLoad(std::string file_name);
    
    Database records_;              // 存储所有记录（二维vector，保留供FP-Tree等算法使用）
    InvertedIndex inverted_index_;  // 倒排索引：元素值 -> 记录索引列表
    size_t record_count_;           // 记录总数（转换前保存）
    size_t max_record_size_;        // 最大记录长度（单条记录中元素最多的）
    int max_num_of_record;          // 记录中的最大数字
};

#endif // DATA_LOADER_HPP

