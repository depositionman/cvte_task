#include "SafeData.h"

// 初始化静态成员变量
std::unique_ptr<SafeData> SafeData::m_instance = nullptr;
std::mutex SafeData::m_mutex;

// 私有构造函数实现
SafeData::SafeData() {
    std::cout << "[SafeData] 构造函数被调用" << std::endl;
    // 初始化数据存储容器
    m_data.clear();
}

// 私有析构函数实现
SafeData::~SafeData() {
    std::cout << "[SafeData] 析构函数被调用" << std::endl;
    // 清理数据
    clearAll();
}

// 获取单例实例
SafeData* SafeData::getInstance() {
    // 双重检查锁定模式（Double-Checked Locking Pattern）
    // 提高性能，避免每次调用都加锁
    if (m_instance == nullptr) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_instance == nullptr) {
            m_instance.reset(new SafeData());
        }
    }
    return m_instance.get();
}

// 存储数据
bool SafeData::setData(const std::string& key, const std::string& value) {
    try {
        std::lock_guard<std::mutex> lock(m_data_mutex);
        m_data[key] = value;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[SafeData] 设置数据失败: " << e.what() << std::endl;
        return false;
    }
}

// 获取数据
bool SafeData::getData(const std::string& key, std::string& value) {
    try {
        std::lock_guard<std::mutex> lock(m_data_mutex);
        auto it = m_data.find(key);
        if (it != m_data.end()) {
            value = it->second;
            return true;
        }
        return false;
    } catch (const std::exception& e) {
        std::cerr << "[SafeData] 获取数据失败: " << e.what() << std::endl;
        return false;
    }
}

// 删除数据
bool SafeData::deleteData(const std::string& key) {
    try {
        std::lock_guard<std::mutex> lock(m_data_mutex);
        auto result = m_data.erase(key);
        return result > 0;
    } catch (const std::exception& e) {
        std::cerr << "[SafeData] 删除数据失败: " << e.what() << std::endl;
        return false;
    }
}

// 清空所有数据
void SafeData::clearAll() {
    try {
        std::lock_guard<std::mutex> lock(m_data_mutex);
        m_data.clear();
    } catch (const std::exception& e) {
        std::cerr << "[SafeData] 清空数据失败: " << e.what() << std::endl;
    }
}

// 获取数据项数量
size_t SafeData::size() const {
    try {
        // 注意：const成员函数中使用mutable互斥锁
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_data_mutex));
        return m_data.size();
    } catch (const std::exception& e) {
        std::cerr << "[SafeData] 获取数据大小失败: " << e.what() << std::endl;
        return 0;
    }
}