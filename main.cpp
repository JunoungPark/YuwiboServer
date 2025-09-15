#include "SslContextManager.h"
#include "ThreadManager.h"
#include "PacketHandler.h"
#include "DatabaseManager.h"
#include <csignal>
#include <iostream>
#include <chrono>
#include <thread>

// 종료 플래그
std::atomic<bool> g_shouldTerminate = false;

// SIGINT 핸들러
void SignalHandler(int signum) {
    g_shouldTerminate = true;
}

int main() {
    // 1️⃣ Signal Handler 등록
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);

    // 2️⃣ PacketHandler 일괄 등록
    if(!SslContextManager::Instance().Init())
    {
        std::cerr << "SSL CTX 생성 실패" << std::endl;
        return -1;
    }

    PacketHandler::Instance().Init();
    
    // 3️⃣ DB 연결 초기화
    if (!DatabaseManager::Instance().Connect("192.168.106.128", "user", "Ryghks00!", "yuwibo_db")) {
        std::cerr << "DB 연결 실패" << std::endl;
        return -1;
    }

    // 4️⃣ ServerService 생성 및 io_uring 초기화
    constexpr uint16_t port = 7777;
    constexpr int maxSessionCount = 1000;

    // 5️⃣ ThreadManager 멀티스레드 io_uring Poll Loop 실행
    ThreadManager::Instance().Launch(port);

    std::cout << "🚀 서버가 " << port << " 포트에서 실행 중입니다. Ctrl+C로 종료하세요." << std::endl;

    // 6️⃣ 종료 시그널까지 대기
    while (!g_shouldTerminate) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "\n🛑 종료 시그널 감지, 서버 종료 중..." << std::endl;

    // 7️⃣ 서비스 및 스레드, DB 정리
    ThreadManager::Instance().ListenerStop();
    while (ThreadManager::Instance().GetListenerRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    ThreadManager::Instance().Stop();

    std::cout << "✅ 서버 종료 완료." << std::endl;
    return 0;
}