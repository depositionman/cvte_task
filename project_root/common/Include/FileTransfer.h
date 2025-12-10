#pragma once
#include <stdint.h>
#include <cstring>
#include <string>
#include <ctime>

// 文件传输系统配置宏
#define FILE_CHUNK_SIZE 1024        // 文件块大小（1KB）
#define MAX_FILE_NAME_LENGTH 256    // 最大文件名长度
#define MAX_TRANSFER_ID_LENGTH 64   // 最大传输ID长度

// 文件块结构体，用于客户端和服务端之间的文件传输
struct FileChunk {
    char userid[20];                       // 用户标识
    int fileIndex;                         // 文件块索引
    int totalChunks;                       // 总块数
    char fileName[MAX_FILE_NAME_LENGTH];   // 文件名
    int fileLength;                        // 文件总长度
    mode_t fileMode;                       // 文件权限
    size_t chunkLength;                    // 当前块大小
    char data[FILE_CHUNK_SIZE];            // 文件块数据
    bool isLastChunk;                      // 是否是最后一个块
    char transferId[MAX_TRANSFER_ID_LENGTH]; // 传输标识符，用于断点续传

    // 默认构造函数
    FileChunk() {
        memset(userid, 0, sizeof(userid));
        fileIndex = 0;
        totalChunks = 0;
        memset(fileName, 0, sizeof(fileName));
        fileLength = 0;
        fileMode = 0644;
        chunkLength = 0;
        memset(data, 0, sizeof(data));
        isLastChunk = false;
        memset(transferId, 0, sizeof(transferId));
    }

    // 带参数的构造函数
    FileChunk(const std::string& userid_, int fileIndex_, int totalChunks_,
             const std::string& fileName_, int fileLength_, mode_t fileMode_ = 0644, bool isLastChunk_ = false)
        : fileIndex(fileIndex_), totalChunks(totalChunks_),
          fileLength(fileLength_), fileMode(fileMode_), chunkLength(0), isLastChunk(isLastChunk_) {
        
        memset(userid, 0, sizeof(userid));
        strncpy(userid, userid_.c_str(), sizeof(userid) - 1);
        
        memset(fileName, 0, sizeof(fileName));
        strncpy(fileName, fileName_.c_str(), sizeof(fileName) - 1);
        
        memset(data, 0, sizeof(data));
        memset(transferId, 0, sizeof(transferId));
    }

    // 带传输ID的构造函数
    FileChunk(const std::string& userid_, int fileIndex_, int totalChunks_,
             const std::string& fileName_, int fileLength_, const std::string& transferId_, 
             mode_t fileMode_ = 0644, bool isLastChunk_ = false)
        : fileIndex(fileIndex_), totalChunks(totalChunks_),
          fileLength(fileLength_), fileMode(fileMode_), chunkLength(0), isLastChunk(isLastChunk_) {
        
        memset(userid, 0, sizeof(userid));
        strncpy(userid, userid_.c_str(), sizeof(userid) - 1);
        
        memset(fileName, 0, sizeof(fileName));
        strncpy(fileName, fileName_.c_str(), sizeof(fileName) - 1);
        
        memset(data, 0, sizeof(data));
        memset(transferId, 0, sizeof(transferId));
        strncpy(transferId, transferId_.c_str(), sizeof(transferId) - 1);
    }
};

// 传输状态结构体，用于断点续传（支持位图记录）
struct TransferStatus {
    int totalChunks;           // 总块数
    int fileLength;            // 文件总长度
    int receivedChunks;        // 已接收块数
    int receivedLength;        // 已接收长度
    int statusCode;            // 状态码（0=正常，1=暂停，2=错误）
    bool isCompleted;          // 是否已完成
    std::vector<bool> chunkBitmap; // 位图：true表示已接收，false表示未接收
    
    // 默认构造函数
    TransferStatus() : totalChunks(0), fileLength(0), receivedChunks(0), 
                              receivedLength(0), statusCode(0), isCompleted(false) {}
    
    // 带参数的构造函数
    TransferStatus(int total, int length) : 
        totalChunks(total), fileLength(length), receivedChunks(0), 
        receivedLength(0), statusCode(0), isCompleted(false) {
        chunkBitmap.resize(total, false);
    }
    
    // 标记块为已接收
    void markChunkReceived(int chunkIndex, int chunkSize) {
        if (chunkIndex >= 0 && chunkIndex < totalChunks && !chunkBitmap[chunkIndex]) {
            chunkBitmap[chunkIndex] = true;
            receivedChunks++;
            receivedLength += chunkSize;
            isCompleted = (receivedChunks == totalChunks);
        }
    }
    
    // 获取缺失的块索引列表
    std::vector<int> getMissingChunks() const {
        std::vector<int> missing;
        for (int i = 0; i < totalChunks; ++i) {
            if (!chunkBitmap[i]) {
                missing.push_back(i);
            }
        }
        return missing;
    }
    
    // 重置为恢复传输状态（保留已接收的块信息）
    void resetForResume() {
        // 重置接收计数和长度，但保留位图信息
        receivedChunks = 0;
        receivedLength = 0;
        
        // 重新计算已接收的块数和长度
        for (int i = 0; i < totalChunks; ++i) {
            if (chunkBitmap[i]) {
                receivedChunks++;
                // 注意：这里无法知道每个块的具体大小，所以长度保持不变
            }
        }
        
        isCompleted = (receivedChunks == totalChunks);
        statusCode = 0; // 重置状态码为正常
    }
};