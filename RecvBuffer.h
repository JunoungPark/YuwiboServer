#pragma once
#include <memory>
#include <vector>
#include <cstdint>
#include "Buffer.h"
class RecvBuffer : public Buffer {
public:
    RecvBuffer(int group,int id,size_t size = 4096);


    // 버퍼 등록용 식별자
    void SetBufferId(int group, int id);
    int GetBufferId() const;
    int GetGroupId() const;

private:
    int _groupId = -1;
    int _bufId = -1;
};