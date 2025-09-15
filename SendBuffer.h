#pragma once
#include "Buffer.h"

class SendBuffer : public Buffer {

public:
    SendBuffer(size_t size = 4096) : Buffer(size){}
};