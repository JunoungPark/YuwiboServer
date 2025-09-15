#include "ThreadManager.h"
#include<algorithm>
#include "Listener.h"
#include <random>

void ThreadManager::Launch(uint16_t port) {
   
    _running = true;
    
    for (size_t i = 0; i < std::max((unsigned)1, std::thread::hardware_concurrency()); ++i) {

        auto worker = std::make_unique<Worker>(i, _running);
        worker->Start();

        if(i==0) 
        {
            std::vector<int> direct_fds_storage(1, -1);
            io_uring_register_files(&worker->GetRing(), direct_fds_storage.data(), direct_fds_storage.size());
            listener.StartAccept(worker->GetRing(),port);
        }
        workers.emplace_back(std::move(worker)); 
    }
}

io_uring& ThreadManager::GetRandomRing()
{ 
    static std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<int> dist(0, workers.size() - 1);

        // 무작위 워커 2개 선택
        auto w1 = workers[dist(rng)].get();
        auto w2 = workers[dist(rng)].get();

        // 세션 수 비교 → 더 적은 워커 선택
        return w1->GetSessionCount() <= w2->GetSessionCount() ? w1->GetRing() :  w2->GetRing();
}

Worker *ThreadManager::GetRandomWorker()
{
    static std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<int> dist(0, workers.size() - 1);

        // 무작위 워커 2개 선택
        auto w1 = workers[dist(rng)].get();
        auto w2 = workers[dist(rng)].get();

        // 세션 수 비교 → 더 적은 워커 선택
        return w1->GetSessionCount() <= w2->GetSessionCount() ? w1 :  w2;
}

void ThreadManager::Stop() {

    _running = false;

    for (auto& w : workers) w->Stop();
    for (auto& w : workers) w->Join();

    if (!workers.empty()) io_uring_unregister_files(&workers[0]->GetRing());
    
}
