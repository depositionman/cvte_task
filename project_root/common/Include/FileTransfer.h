#pragma once
#include <stdint.h>
#include <cstring>
#include <string>

// 文件块结构体，用于客户端和服务端之间的文件传输
struct FileChunk {
    char userid[20];       // 用户标识
    int fileIndex;         // 文件块索引
    int totalChunks;       // 总块数
    char fileName[256];    // 文件名
    int fileLength;        // 文件总长度
    mode_t fileMode;       // 文件权限
    size_t chunkLength;    // 当前块大小
    char data[1024];       // 文件块数据
    bool isLastChunk;      // 是否是最后一个块

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
    }
};