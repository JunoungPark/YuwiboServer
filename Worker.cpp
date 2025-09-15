#include "Worker.h"
#include <arpa/inet.h>
#include "ThreadManager.h"
#include <sys/eventfd.h>
#include "poll.h"
#include "RequestContext.h"

void Worker::Start()
{
    int ret = io_uring_queue_init(256, &ring, 0);
    if (ret < 0)
    {
        std::cerr << "io_uring_queue_init failed for worker " << workerId << "\n";
        return;
    }

    int pending = 0;

    std::cout << "Worker " << workerId << " started\n";

    RegisterBufRing();
    running = true;
    workerThread = std::thread(&Worker::Run, this);
}

void Worker::RegisterBufRing()
{
    constexpr size_t kBufSize = 4096;

    _recvBufferPool.reserve(BufCount);
    for (int i = 0; i < BufCount; ++i)
    {
        _recvBufferPool.emplace_back(std::move(std::make_unique<RecvBuffer>(BufGroupId, i)));
    }

    // BUF_RING 메모리 생성 (4096 바이트 정렬 필수!)
    size_t ringSize = BufCount * sizeof(struct io_uring_buf);
    void *bufRingPtr = aligned_alloc(4096, ringSize);
    memset(bufRingPtr, 0, ringSize); // 권장: 클리어

    _bufRing = static_cast<io_uring_buf_ring *>(bufRingPtr); // 멤버 변수로 보관 추천

    // 💡 io_uring_buf_reg 설정
    struct io_uring_buf_reg reg = {};
    reg.ring_addr = (unsigned long)_bufRing;
    reg.ring_entries = BufCount;
    reg.bgid = BufGroupId;
    reg.resv[0] = 0; // reserved, must be 0
    reg.resv[1] = 0;

    // 💥 BUF_RING 등록
    int ret = io_uring_register_buf_ring(&ring, &reg, 0);
    if (ret < 0)
    {
        std::cerr << "register_buf_ring failed: " << strerror(-ret) << "\n";
        std::terminate(); // 혹은 예외
    }

    // 🧃 버퍼 제공
    for (int i = 0; i < BufCount; ++i)
    {
        io_uring_buf_ring_add(_bufRing, (void *)_recvBufferPool[i]->GetBuffer(), _recvBufferPool[i]->GetBufferSize(), i, BufCount-1, i);
    }
    io_uring_buf_ring_advance(_bufRing, BufCount);
}

void Worker::Stop()
{
    sessionManager.Close(ring);
    StopSQE();
}

void Worker::Join()
{
    if (workerThread.joinable())
        workerThread.join();

    free(_bufRing);

    io_uring_unregister_buf_ring(&ring, BufGroupId);

    io_uring_queue_exit(&ring);
}

void Worker::Run()
{
    std::cerr << "Worker_Run : " << workerId << std::endl;

    while (true)
    {
        io_uring_cqe *cqe = nullptr;

        if (io_uring_wait_cqe(&ring, &cqe) < 0)
        {
            perror("io_uring_wait_cqe");
            continue;
        }

        if (auto context_ptr = static_cast<RequestContext *>(io_uring_cqe_get_data(cqe)))
        {
            if (!running && sessionManager.GetSize() == 0)
            {
                std::unique_ptr<RequestContext> context(context_ptr);
                break;
            }
        }

        // CQE 처리
        HandleCompletion(*cqe);

        io_uring_cqe_seen(&ring, cqe);
    }

    std::cerr << "Worker_Run Complete : " << workerId << "\n";
}

void Worker::HandleCompletion(io_uring_cqe &cqe)
{
    auto requestContext = static_cast<RequestContext *>(io_uring_cqe_get_data(&cqe));
    if (!requestContext)
        return;
    const auto &type = requestContext->type;

    switch (type)
    {
    case REQ_TYPE_ACCEPT:
    {
        if (cqe.res < 0)
        {
            // 오류 발생
            int error_code = -cqe.res;
            std::cerr << "io_uring operation Accept failed: " << strerror(error_code) << std::endl;
        }
        else
            std::cout << "io_uring operation Accept Success" << std::endl;

        OnAcceptCompleted(cqe);
        return;
    }
    break;
    case REQ_TYPE_RECV:
        if (auto session = static_cast<SessionContext *>(requestContext)->session)
        {
            if (cqe.res < 0)
            {
                // 오류 발생
                int error_code = -cqe.res;
                std::cerr << "io_uring operation Recv failed: " << strerror(error_code) << std::endl;
            }
            else if(cqe.res == 0) 
                std::cout << "io_uring operation Connection Closed" << std::endl;
            else
                std::cout << "io_uring operation Recv Success" << std::endl;

            session->OnRecv(cqe);
            return;
        }
        break;
    }

    if (cqe.res < 0)
    {
        // 오류 발생
        int error_code = -cqe.res;
        std::cerr << "io_uring operation failed: " << strerror(error_code) << std::endl;
    }
    else
        std::cout << "io_uring operation Success" << std::endl;

    std::unique_ptr<RequestContext> context(requestContext);

    switch (type)
    {
    case REQ_TYPE_SOCKET_SETUP_CHAIN_START:
        std::cout << "Server socket setup chain completed successfully." << std::endl;
        ThreadManager::Instance().OnListenerStart();
        break;
    case REQ_TYPE_ACCEPT_CANCEL:
        std::cout << "Listener Cancel" << std::endl;
        break;
    case REQ_TYPE_RECV_CANCEL:
        if (auto session = static_cast<SessionContext *>(context.get())->session)
            session->Close(true);
        break;
    case REQ_TYPE_SESSION_CLOSE:
        sessionManager.RemoveSession(static_cast<SessionCloseContext *>(context.get())->fd);
        StopSQE();
        break;
    case REQ_TYPE_SEND:
        if (auto session = static_cast<SessionContext *>(context.get())->session)
            session->OnSend(cqe.res);
        break;
    case REQ_TYPE_LISTENER_CLOSE:
        std::cout << "Listener Stop" << std::endl;
        ThreadManager::Instance().OnListenerClosed();
        break;
    }
}

void Worker::OnAcceptCompleted(io_uring_cqe &cqe)
{

    int clientFd = cqe.res; // cqe->res에 일반 리눅스 FD가 들어있음

    if (clientFd < 0)
    {
        std::cerr << "Accept 실패: " << strerror(-clientFd) << std::endl;
        // 에러 처리: 멀티샷이므로 계속 대기할지, 재시도할지 결정
        return;
    }

    sockaddr_in addr;
    socklen_t addrLen = sizeof(addr);

    if (getpeername(clientFd, (sockaddr *)&addr, &addrLen) == 0)
    {
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));

        std::cout << "새 클라이언트 연결 (일반 FD): " << clientFd
                  << ", IP: " << ip
                  << ", Port: " << ntohs(addr.sin_port) << std::endl;
    }

    auto w = ThreadManager::Instance().GetRandomWorker();
    auto session = std::make_unique<Session>(clientFd);

    w->AddSession(session.release());
}

void Worker::AddSession(Session *session)
{
    session->SetWorker(this);
    sessionManager.AddSession(session);
}

void Worker::StopSQE()
{
    if (running || sessionManager.GetSize() != 0)
        return;

    io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    if (!sqe)
        return;

    auto context = std::make_unique<RequestContext>(REQ_TYPE_WORKER_STOP);

    io_uring_prep_nop(sqe);
    io_uring_sqe_set_data(sqe, context.release()); // 특별한 값 설정
    io_uring_submit(&ring);
}
