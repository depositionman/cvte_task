#pragma once
#include "TestData.h"
#include "FileTransfer.h"
#include <cstdint>

class ITestService {
public:
    virtual ~ITestService() = default;
    // Set接口
    virtual bool SetTestBool(bool param) = 0;
    virtual bool SetTestInt(int param) = 0;
    virtual bool SetTestDouble(double param) = 0;
    virtual bool SetTestString(const std::string& param) = 0;
    virtual bool SetTestInfo(const TestInfo& param) = 0;
    // Get接口
    virtual bool GetTestBool() = 0;  
    virtual int GetTestInt() = 0;
    virtual double GetTestDouble() = 0;
    virtual std::string GetTestString() = 0; 
    virtual TestInfo GetTestInfo() = 0;
    // 文件传输接口
    virtual bool SendFileChunk(const FileChunk& chunk) = 0;
    // 断点续传接口
    virtual TransferStatus GetTransferStatus(const std::string& transferId, const std::string& userid, const std::string& fileName) = 0;
    virtual std::vector<int> GetMissingChunks(const std::string& transferId, const std::string& userid, const std::string& fileName) = 0;
    virtual bool ResumeTransfer(const std::string& transferId, const std::string& userid, const std::string& fileName, int startChunk) = 0;
};