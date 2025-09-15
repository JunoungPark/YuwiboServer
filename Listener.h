#pragma once
#include <atomic>
#include <memory>

class Listener
{
public:
    bool StartAccept(class io_uring &ring, uint16_t port);
    void StartStop(io_uring &ring);

    inline bool GetRunning() { return Listener_running; }
    inline void SetRunning(bool IsTrue) { Listener_running = IsTrue; }

private:
    std::unique_ptr<class RequestContext> accept_ptr;
    std::atomic<bool> Listener_running = false;
};