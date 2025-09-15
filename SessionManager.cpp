#include "SessionManager.h"

void SessionManager::Close(io_uring &ring)
{

    if (_sessions.empty())
        return;

    for (auto &session : _sessions)
        session.second->StartClose(false);

    io_uring_submit(&ring);
}

void SessionManager::AddSession(Session *session)
{
    _sessions[session->GetFd()] = std::unique_ptr<Session>(session);
    session->Strat();
}

void SessionManager::RemoveSession(int fd)
{
    _sessions.erase(fd);
    std::cout<< "Session " << fd << " Close" << std::endl;

}
