#pragma once
#include "TestData.h"

class ITestListener {
public:
    virtual ~ITestListener() = default;
    virtual void OnTestBoolChanged(bool param) = 0;
    virtual void OnTestIntChanged(int param) = 0;
    virtual void OnTestDoubleChanged(double param) = 0;
    virtual void OnTestStringChanged(const std::string& param) = 0;
    virtual void OnTestInfoChanged(const TestInfo& param) = 0;
    virtual void OnFileTransferProgress(const std::string& file_id, size_t progress, size_t total) = 0; // 新增传输进度广播
};