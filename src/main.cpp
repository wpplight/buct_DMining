#include "dataload/data_loader.hpp"
#include "apriori/apr.hpp"
#include "fptree/fp.hpp"
#include <iostream>
#include <chrono>

using std::cout;
using std::cin;
using std::endl;

int main() {
    // 总开始时间
    auto total_start = std::chrono::high_resolution_clock::now();

    cout << "========== 算法性能测试 ==========" << endl;
    cout << "请输入并发数量: ";
    int co;
    cin >> co;
    
    cout<<"请输入置信度: ";
    double confidence;
    cin >> confidence;

    cout<<"检验哪种算法： 1.Apriori 2.FPTree ";
    int choose;
    cin>>choose;

    

    // 数据加载和转换计时
    auto data_load_start = std::chrono::high_resolution_clock::now();
    
    // 创建数据加载器，构造函数会自动加载数据并转换为倒排索引
    // 传入线程数以支持并发转换
    DataLoader loader("retail.csv", ' ', co);
    
    auto data_load_end = std::chrono::high_resolution_clock::now();
    auto data_load_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        data_load_end - data_load_start
    );

    
    cout << "\n数据加载和转换完成！" << endl;
    cout << "  - 记录总数: " << loader.size() << endl;
    cout << "  - 最大元素值: " << loader.getMaxValue() << endl;
    cout << "  - 耗时: " << data_load_duration.count() << " ms" << endl;


    if (choose == 1) {
    // Apriori 算法
    cout << "\n使用置信度: " << confidence << endl;

    // Apriori 算法计时
    auto apriori_start = std::chrono::high_resolution_clock::now();
    
    // 创建 Apriori 实例并执行算法
    Apriori apriori(loader, confidence, co);
    
    auto apriori_end = std::chrono::high_resolution_clock::now();
    auto apriori_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        apriori_end - apriori_start
    );


    // 输出结果
    cout << "\n========== 性能统计 ==========" << endl;
    cout << "数据加载和转换时间: " << data_load_duration.count() << " ms" << endl;
    cout << "Apriori 算法时间: " << apriori_duration.count() << " ms" << endl;
    cout<<"total time: "<<data_load_duration.count()+apriori_duration.count()<<" ms"<<endl;
    cout << "================================" << endl;
    
    // 显示所有level的结果
    apriori.displayAllLevels(); 
    } else if (choose == 2) {
        // FP-Tree 算法
        cout << "\n使用最小支持度: " << confidence << endl;
        
        // FP-Tree 算法计时
        auto fptree_start = std::chrono::high_resolution_clock::now();
        
        FPTree fptree(loader, confidence);
        
        auto fptree_end = std::chrono::high_resolution_clock::now();
        auto fptree_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            fptree_end - fptree_start
        );

        auto& frequent_itemsets = fptree.levels;
        int lc = 0;
        for(const auto& level : frequent_itemsets){
            if(level.empty()) break;

            cout << "level: " << lc << " " << level.size() << endl;
            lc++;
        }
        
        // 输出结果
        cout << "\n========== 性能统计 ==========" << endl;
        cout << "数据加载和转换时间: " << data_load_duration.count() << " ms" << endl;
        cout << "FP-Tree 算法时间: " << fptree_duration.count() << " ms" << endl;
        cout << "total time: " << data_load_duration.count() + fptree_duration.count() << " ms" << endl;
        cout << "================================" << endl;
    }
    return 0;
}