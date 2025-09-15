#pragma once
#include <unordered_map>
#include <memory>
#include <mutex>
#include "Session.h"

class SessionManager
{
public:
    void Close(io_uring& ring);

    void AddSession(Session *session);
    void RemoveSession(int fd);
    int GetSize() { return _sessions.size(); }

private:
    std::unordered_map<int, std::unique_ptr<Session>> _sessions;
};
