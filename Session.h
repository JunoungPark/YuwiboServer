#pragma once
#include <memory>
#include <vector>
#include <queue>
#include <string>
#include <mutex>
#include <liburing.h>
#include "RecvBuffer.h"
#include "SendBuffer.h"
#include <google/protobuf/message.h>
#include "TLS.h"
#include "RequestContext.h"

class Session {
public:
    Session(int fd);

    void Strat();
    void StartRecv();
    void Send(const std::string& data); 
    void Send(const google::protobuf::Message& message);
    void HandlePacket(const char* data, size_t len);
    void StartClose(bool submit);
    void Close(bool AdditionalCheck);
    void OnRecv(io_uring_cqe &cqe);
    void OnSend(ssize_t bytes);
    
    void SetWorker(class Worker* _worker) {worker=_worker;}
    void SuccessLogin(std::string user_id){ _user_id=user_id;}

    int GetFd() const { return _fd; }
    std::string GetUserId() const {return _user_id;}
    int GetRoomId() const {return _room_id;}
    void SetRoomId(uint id) {_room_id = id;}
private:
    int _fd;

    Worker* worker = nullptr;
    TLS _tls;
    int _pendingRecvBufId;

    std::queue<std::unique_ptr<SendBuffer>> _sendQueue;
    std::atomic<uint16_t> SendCount = 0;

    std::string _user_id;
    int _room_id=-1;

    std::unique_ptr<SessionContext> session_recv_ptr;

    bool canceling=false;
    bool _isSending = false;
    bool closing = false;

    bool TLSHandshake();
    void OnTLSHandshakeComplete();

    void FlushTLSWriteBuffer();

    void SendInternal();
    bool ProcessPacket(Buffer &buffer);
};
