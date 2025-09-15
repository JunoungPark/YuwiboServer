#include "Room.h"
#include "SendBuffer.h"
#include "DatabaseManager.h"
#include "RoomManager.h"
bool Room::Enter(UnrealEngineMessage::JoinRoomResponse &joinroomResponse, Session *session, bool isCreating)
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_sessions.size() + 1 > info.max_player_count)
    {
        joinroomResponse.set_success(false);
        return false;
    }

    _sessions.insert(session);
    session->SetRoomId(info.id);

    joinroomResponse.set_success(true);
    joinroomResponse.set_error_code(UnrealEngineMessage::ErrorCode::NONE);
    joinroomResponse.set_my_id(session->GetUserId());

    auto roominfo = joinroomResponse.mutable_room();
    roominfo->set_id(info.id);
    roominfo->set_room_name(info.roomName);
    roominfo->set_current_player_count(_sessions.size());
    roominfo->set_max_player_count(info.max_player_count);
    roominfo->set_host_id(info.hostId);

    for (auto &_session : _sessions)
    {
        auto res = DatabaseManager::Instance().GetUserInfo(_session->GetUserId());

        if (res.success)
        {
            auto userinfo = joinroomResponse.add_users();
            userinfo->set_user_id(_session->GetUserId());
            userinfo->set_user_name(res.user_name);
        }
    }

    if(_sessions.size() > 1) UpdateRoomBroadcast(session);

    return true;
}

void Room::Leave(Session *session)
{
    std::lock_guard<std::mutex> lock(_mutex);

    _sessions.erase(session);

    session->SetRoomId(-1);

    if (!_sessions.size())
    {
        RoomManager::Instance().RemoveRoom(info.id);
        return;
    }

    if(session->GetUserId() == info.hostId) info.hostId = (*_sessions.begin())->GetUserId();

    UpdateRoomBroadcast();
}

void Room::Broadcast(Session *_session, std::string content)
{
    std::lock_guard<std::mutex> lock(_mutex);

    UnrealEngineMessage::BroadcastResponse message;

    auto userinfo = message.mutable_userinfo();

    auto res = DatabaseManager::Instance().GetUserInfo(_session->GetUserId());

    if (res.success)
    {
        userinfo->set_user_name(res.user_name);

        message.set_message(content);

        for (auto &session : _sessions)
        {
            session->Send(message);
        }
    }
}

void Room::StartGame(std::string url,Session* ignoredSession)
{
    UnrealEngineMessage::GameResponse message;
    
    for (auto &session : _sessions)
    {
        if(ignoredSession==session) continue;

        message.set_url(url);
        session->Send(message);
    }
}

void Room::UpdateRoomBroadcast(Session* ignoredSession)
{
    UnrealEngineMessage::UpdateRoomInfo message;

    auto updateinfo = message.mutable_room();
    updateinfo->set_id(info.id);
    updateinfo->set_room_name(info.roomName);
    updateinfo->set_current_player_count(_sessions.size());
    updateinfo->set_max_player_count(info.max_player_count);

    for (auto &_session : _sessions)
    {
        auto res = DatabaseManager::Instance().GetUserInfo(_session->GetUserId());

        if (res.success)
        {
            auto userinfo = message.add_users();
            userinfo->set_user_id(_session->GetUserId());
            userinfo->set_user_name(res.user_name);
        }
    }

    for (auto &session : _sessions)
    {
        if(ignoredSession==session) continue;

        message.set_my_id(session->GetUserId());
        session->Send(message);
    }
}