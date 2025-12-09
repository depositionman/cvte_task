#pragma once
#include <iostream>
#include <map>
#include <string>
#include <mutex>
#include <memory>

// 线程安全的数据存储类（单例模式）
// 用于在多线程环境下安全地存储和访问共享数据
class SafeData {
private:
    // 单例实例
    static std::unique_ptr<SafeData> m_instance;
    // 互斥锁，保证线程安全
    static std::mutex m_mutex;
    
    // 数据存储容器，使用std::map存储键值对
    std::map<std::string, std::string> m_data;
    // 数据操作互斥锁
    std::mutex m_data_mutex;
    
    // 私有构造函数，防止外部实例化
    SafeData();
    // 私有析构函数
    ~SafeData();
    
    // 禁止拷贝构造和赋值操作
    SafeData(const SafeData&) = delete;
    SafeData& operator=(const SafeData&) = delete;
    
    // 友元类声明，允许std::default_delete访问析构函数
    friend class std::default_delete<SafeData>;
    
public:
    // 获取单例实例
    static SafeData* getInstance();
    
    // 存储数据
    bool setData(const std::string& key, const std::string& value);
    
    // 获取数据
    bool getData(const std::string& key, std::string& value);
    
    // 删除数据
    bool deleteData(const std::string& key);
    
    // 清空所有数据
    void clearAll();
    
    // 获取数据项数量
    size_t size() const;
};