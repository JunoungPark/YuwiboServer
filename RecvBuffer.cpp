#include "RecvBuffer.h"
#include <cstring>

RecvBuffer::RecvBuffer(int group, int id, size_t size) : Buffer(size), _groupId(group), _bufId(id) {}

void RecvBuffer::SetBufferId(int group, int id)
{
    _groupId = group;
    _bufId = id;
}

int RecvBuffer::GetBufferId() const
{
    return _bufId;
}

int RecvBuffer::GetGroupId() const
{
    return _groupId;
}
