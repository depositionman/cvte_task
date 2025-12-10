#pragma once
#include "ITestService.h"
#include "ITestListener.h"
#include "FileTransfer.h"
#include <vector>
#include <mutex>

class TestService : public ITestService {
public:
    TestService() = default;
    ~TestService() = default;

    // ITestService接口实现
    bool SetTestBool(bool param) override;
    bool SetTestInt(int param) override;
    bool SetTestDouble(double param) override;
    bool SetTestString(const std::string& param) override;
    bool SetTestInfo(const TestInfo& param) override;

    bool GetTestBool() override;
    int GetTestInt() override;
    double GetTestDouble() override;
    std::string GetTestString() override;
    TestInfo GetTestInfo() override;

    bool SendFileChunk(const FileChunk& chunk) override;
    
    // 断点续传接口
    TransferStatus GetTransferStatus(const std::string& transferId, const std::string& userid, const std::string& fileName) override;
    std::vector<int> GetMissingChunks(const std::string& transferId, const std::string& userid, const std::string& fileName) override;
    bool ResumeTransfer(const std::string& transferId, const std::string& userid, const std::string& fileName, int startChunk) override;

    // 注册观察者
    void registerListener(ITestListener* listener);

private:
    // 广播辅助函数
    void broadcastTestBoolChanged(bool param);
    void broadcastTestIntChanged(int param);
    void broadcastTestDoubleChanged(double param);
    void broadcastTestStringChanged(const std::string& param);
    void broadcastTestInfoChanged(const TestInfo& param);

    std::vector<ITestListener*> listeners_;  // 观察者列表
    std::mutex listener_mutex_;              // 观察者列表锁
};