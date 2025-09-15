#pragma once
#include <vector>
#include <cstdint>
#include <mutex>
#include "memory"

class Buffer {
public:
    Buffer(size_t size = 4096);
    void Write(const char* data, size_t len);
    size_t Read(char* dest, size_t len);

    char* GetBuffer();
    char* GetWriteBuffer();
    const char* GetReadBuffer() const;

    size_t GetBufferSize() const;    // 전체 버퍼 크기
    size_t GetStoredSize() const;
    int GetRemainingSize() const;

    void OnWrite(size_t bytes);      // io_uring에서 수신 완료 후 호출
    void OnRead(size_t bytes);

    // 상태 관리 
    void Append(const Buffer& other);
    void PreserveFrom(size_t offset);
    void Reset();
    void Resize(size_t size);        // 버퍼 크기 재조정 (풀 관리용)
protected:
    std::vector<char> _buffer;
    size_t _writePos = 0;
    size_t _readPos = 0;
    mutable std::mutex _mutex;
};