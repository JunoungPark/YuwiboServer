#pragma once
#include <unordered_set>
#include <memory>
#include "UnrealEngineMessage.pb.h"
#include "Session.h"

struct RoomInfo
{
    uint id;
    std::string roomName; // 방의 이름
    uint32_t max_player_count;
    std::string hostId;
};

class Room
{
public:
    Room(uint id, std::string roomName,uint32_t max_player_count,std::string hostid) : info{id, roomName,max_player_count,hostid}{}

    bool Enter(UnrealEngineMessage::JoinRoomResponse& joinroomResponse, Session* session, bool isCreating=false);
    void Leave(Session* session);
    void Broadcast(Session* _session, std::string content);
    void StartGame(std::string url, Session* ignoredSession=nullptr);

    bool CompareRoomName(std::string roomName) { return info.roomName.find(roomName) != std::string::npos; }
    RoomInfo GetRoomInfo() { return info; }
    size_t GetCurrentPlayerCount() { return _sessions.size(); }
private:
    void UpdateRoomBroadcast(Session* ignoredSession=nullptr);
private:

    std::unordered_set<Session*> _sessions;
    RoomInfo info;

    mutable std::mutex _mutex;
};
