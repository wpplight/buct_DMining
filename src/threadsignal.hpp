// ThreadPoolSingleton.hpp
#pragma once
#include "bs.hpp"
#include <memory>
#include <mutex>

/**
 * 全局单例线程池
 * 提供线程安全的全局线程池访问
 * 使用单例模式确保整个程序只有一个线程池实例
 */
class ThreadPoolSingleton {
public:
    /**
     * 获取线程池单例实例
     * @param thread_count 线程数量，如果为0则使用默认值（通常是硬件并发数）
     * @return 线程池的引用
     */
    static BS::thread_pool<>& getInstance(size_t thread_count = 0) {
        static std::once_flag once_flag;
        static std::unique_ptr<BS::thread_pool<>> instance;
        
        std::call_once(once_flag, [thread_count]() {
            if (thread_count > 0) {
                instance = std::make_unique<BS::thread_pool<>>(thread_count);
            } else {
                // 使用默认线程数（通常是硬件并发数）
                instance = std::make_unique<BS::thread_pool<>>();
            }
        });
        
        return *instance;
    }
    
    /**
     * 获取线程池单例实例（const版本）
     */
    static const BS::thread_pool<>& getConstInstance() {
        return getInstance();
    }
    
    /**
     * 重置线程池（销毁当前实例，下次调用getInstance时会创建新实例）
     * 注意：只有在确定没有其他代码在使用线程池时才应该调用此方法
     */
    static void reset() {
        // 注意：由于使用了静态局部变量，实际上无法真正"重置"
        // 这个方法主要是为了接口完整性
        // 如果需要真正的重置功能，需要使用不同的实现方式
    }
    
    // 禁止拷贝和移动
    ThreadPoolSingleton(const ThreadPoolSingleton&) = delete;
    ThreadPoolSingleton& operator=(const ThreadPoolSingleton&) = delete;
    ThreadPoolSingleton(ThreadPoolSingleton&&) = delete;
    ThreadPoolSingleton& operator=(ThreadPoolSingleton&&) = delete;

private:
    // 禁止外部创建实例
    ThreadPoolSingleton() = default;
    ~ThreadPoolSingleton() = default;
};

// 便捷的全局访问函数
/**
 * 获取全局线程池实例（便捷函数）
 * @param thread_count 线程数量，仅在第一次调用时有效
 * @return 线程池的引用
 */
inline BS::thread_pool<>& getThreadPool(size_t thread_count = 0) {
    return ThreadPoolSingleton::getInstance(thread_count);
}

