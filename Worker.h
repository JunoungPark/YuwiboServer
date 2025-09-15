#pragma once
#include <liburing.h>
#include <thread>
#include <atomic>
#include <vector>
#include <iostream>
#include "SessionManager.h"


class Worker
{
public:
    Worker(int id, std::atomic<bool> &_running) : workerId(id), running(_running) {}

    void Start();

    void Stop();

    void Join();

    io_uring &GetRing() { return ring; }

    void AddSession(Session* session);

    int GetSessionCount() { return sessionManager.GetSize(); }

    io_uring_buf_ring *GetBufRing() { return _bufRing; }
    __u16 GetBufGroupId() { return BufGroupId; }
    __u16 GetBufCount() { return BufCount; }
    RecvBuffer *GetRecvBuffer(unsigned short bufId) { return _recvBufferPool[bufId].get(); }

private:
    void RegisterBufRing();
    void Run();
    void HandleCompletion(io_uring_cqe &cqe);
    void OnAcceptCompleted(io_uring_cqe &cqe);
    void StopSQE();
private:
    int workerId;
    __u16 BufGroupId = 42;
    size_t BufCount = 128;

    io_uring ring;
    io_uring_buf_ring *_bufRing;
    std::thread workerThread;
    std::vector<std::unique_ptr<RecvBuffer>> _recvBufferPool;
    SessionManager sessionManager;

    std::atomic<bool> &running;
};