#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

// 测试数据结构体
struct TestInfo {
    bool bool_param;
    int int_param;
    double double_param;
    std::string string_param;

    // 默认构造函数
    TestInfo() : bool_param(false), int_param(0), double_param(0.0), string_param("") {}

    // 带参数的构造函数
    TestInfo(bool b, int i, double d, const std::string& s)
        : bool_param(b), int_param(i), double_param(d), string_param(s) {}
};

// JSON序列化宏
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
    TestInfo,
    bool_param,
    int_param,
    double_param,
    string_param
);