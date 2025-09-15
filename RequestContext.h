#pragma once
#include "string.h"
#include "memory"
#include <netinet/in.h>

class Session;

enum RequestType
{
    REQ_TYPE_CREATE_SOCKET,
    REQ_TYPE_SOCKET_SETUP_CHAIN_START, // 소켓 생성, 바인드, 리슨 체인
    REQ_TYPE_ACCEPT,
    REQ_TYPE_ACCEPT_CANCEL, // 클라이언트 accept
    REQ_TYPE_SESSION_CLOSE,
    REQ_TYPE_SEND,
    REQ_TYPE_RECV,
    REQ_TYPE_RECV_CANCEL,
    REQ_TYPE_LISTENER_CLOSE,
    REQ_TYPE_WORKER_STOP,
    // ... 다른 요청 타입 (read, write 등)
};

// 모든 io_uring 요청에 대한 기본 컨텍스트
struct RequestContext
{
    RequestContext(RequestType _type) : type(_type) {};
    
    RequestType type;
};

// 소켓 설정 체인을 위한 특수 컨텍스트 (한 번만 사용되고 해제됨)
struct SocketSetupChainContext : public RequestContext
{

    SocketSetupChainContext(int port) : RequestContext(REQ_TYPE_SOCKET_SETUP_CHAIN_START)
    {
        memset(&bind_addr, 0, sizeof(bind_addr));
        bind_addr.sin_family = AF_INET;
        bind_addr.sin_port = htons(port);
        bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    }

    int setsockopt_opt_val = 1; // SO_REUSEADDR 값을 여기에 저장하여 수명 관리
    sockaddr_in bind_addr;      // bind 주소 정보를 여기에 저장하여 수명 관리
};

struct SessionContext : public RequestContext
{

    SessionContext(Session* _session, RequestType _type) : RequestContext(_type), session(_session) {}

    Session* session;
    // std::shared_ptr<Session> session_ptr; // 생성된 세션 객체를 여기에 저장 (선택 사항)
};

struct SessionSendContext : public SessionContext
{

    SessionSendContext(Session* _session, const char *src, size_t _len, RequestType _type) : SessionContext(_session, _type), data(new char[_len]), len(_len) { memcpy(data.get(), src, len); }

    std::unique_ptr<char[]> data;
    size_t len;
    // std::shared_ptr<Session> session_ptr; // 생성된 세션 객체를 여기에 저장 (선택 사항)
};

struct SessionCloseContext : public RequestContext
{
    SessionCloseContext(int _fd) : RequestContext(REQ_TYPE_SESSION_CLOSE), fd(_fd) {}

    int fd;
    // std::shared_ptr<Session> session_ptr; // 생성된 세션 객체를 여기에 저장 (선택 사항)
};
