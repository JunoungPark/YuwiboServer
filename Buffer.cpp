#include "Buffer.h"
#include <cstring>

Buffer::Buffer(size_t size) : _buffer(size) {}

void Buffer::Write(const char* data, size_t len) {
    std::lock_guard<std::mutex> lock(_mutex);
    size_t space = _buffer.size() - _writePos;
    if (len > space) {
        _buffer.resize(_writePos + len); 
    }
    memcpy(&_buffer[_writePos], data, len);
    _writePos += len;
}

size_t Buffer::Read(char* dest, size_t len) {
    std::lock_guard<std::mutex> lock(_mutex);
    size_t stored = _writePos - _readPos;
    size_t toRead = (len < stored) ? len : stored;
    memcpy(dest, &_buffer[_readPos], toRead);
    _readPos += toRead;
    if (_readPos == _writePos) {
        _readPos = _writePos = 0;
    }
    return toRead;
}

char* Buffer::GetBuffer()
{
    std::lock_guard<std::mutex> lock(_mutex);
    
    return  _buffer.data();
}

char *Buffer::GetWriteBuffer()
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _buffer.data()+_writePos;
}

const char* Buffer::GetReadBuffer() const {
    std::lock_guard<std::mutex> lock(_mutex);
        return _buffer.data() + _readPos;
}

size_t Buffer::GetBufferSize() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _buffer.size();
}


size_t Buffer::GetStoredSize() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _writePos - _readPos;
}

int Buffer::GetRemainingSize() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _buffer.size()-_writePos;
}

void Buffer::OnWrite(size_t bytes) {
    std::lock_guard<std::mutex> lock(_mutex);
    _writePos += bytes;
    _readPos = 0;
}

void Buffer::OnRead(size_t bytes) {
    std::lock_guard<std::mutex> lock(_mutex);
    _readPos += bytes;
    if (_readPos >= _writePos) {
        _readPos = 0;
        _writePos = 0;
    }
}


void Buffer::Append(const Buffer& other)
{ 
    std::lock_guard<std::mutex> lock(_mutex);

    size_t appendSize = other.GetStoredSize();
    const char* appendData = other.GetReadBuffer();

    // 공간이 충분한지 확인 (필요시 resize 또는 assert)
    if (_writePos + appendSize > _buffer.size()) {
        _buffer.resize(_writePos + appendSize);
    }

    memcpy(_buffer.data() + _writePos, appendData, appendSize);
    _writePos += appendSize;

}

void Buffer::PreserveFrom(size_t offset)
{
    std::lock_guard<std::mutex> lock(_mutex);

    size_t remaining = _writePos - offset;
    memmove(_buffer.data(), _buffer.data() + offset, remaining);
    _readPos = 0;
    _writePos = remaining;
}

void Buffer::Reset() {
    std::lock_guard<std::mutex> lock(_mutex);
    _readPos = _writePos = 0;
}

void Buffer::Resize(size_t size)
{
    std::lock_guard<std::mutex> lock(_mutex);

    _buffer.resize(size);
    Reset();
}