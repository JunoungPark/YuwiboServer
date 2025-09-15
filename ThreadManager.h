#pragma once
#include <thread>
#include <vector>
#include <functional>
#include <atomic>
#include "Worker.h"
#include "Listener.h"

class ThreadManager
{
private:
    ThreadManager() = default;

public:
    ThreadManager(const ThreadManager &) = delete;            // 복사 방지
    ThreadManager &operator=(const ThreadManager &) = delete; // 대입 방지

    inline static ThreadManager &Instance()
    {
        static ThreadManager instance;
        return instance;
    }

    void Launch(uint16_t port);

    io_uring &GetRandomRing();
    Worker *GetRandomWorker();

    void ListenerStop() { listener.StartStop(workers[0]->GetRing());}
    void OnListenerStart() { listener.SetRunning(true); }
    void OnListenerClosed() { listener.SetRunning(false); }
    bool GetListenerRunning() {return listener.GetRunning(); }

    void Stop();

private:
    Listener listener;
    std::vector<std::unique_ptr<Worker>> workers;
    std::atomic<bool> _running = false;
};
