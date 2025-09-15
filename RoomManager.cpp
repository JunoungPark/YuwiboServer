#include "RoomManager.h"

void RoomManager::CreateRoom(UnrealEngineMessage::JoinRoomResponse &joinroomResponse, const UnrealEngineMessage::CreateRoomRequest &message, Session* session)
{
    std::lock_guard<std::mutex> lock(_mutex);
 
    uint key = 0;
    while (true)
    {
        // find() 메서드를 사용하여 key가 맵에 있는지 확인
        if (_rooms.find(key) == _rooms.end())
        {
            // key를 찾지 못했으므로, 이 key가 사용 가능
            break;
        }
        key++; // 다음 키로 넘어감
    }

    // 찾은 키를 사용하여 새로운 방 등록
    _rooms[key] = std::make_unique<Room>(key, message.room_name(), message.max_player_count(), session->GetUserId());
    _rooms[key]->Enter(joinroomResponse, session, true);

}

void RoomManager::RemoveRoom(int roomId)
{
    std::lock_guard<std::mutex> lock(_mutex);
    
    _rooms.erase(roomId);
}

Room* RoomManager::GetRoom(int roomId)
{
    auto it = _rooms.find(roomId);
    if (it != _rooms.end())
    {
        return it->second.get();
    }
    return nullptr;
}

void RoomManager::GetRoom(UnrealEngineMessage::FindRoomResponse &findroomResponse, std::string roomName)
{
    for (auto &[id, room] : _rooms)
    {
        if (room->CompareRoomName(roomName))
        {
            auto roominfo = findroomResponse.add_rooms();

            auto info = room->GetRoomInfo();
            roominfo->set_id(id);
            roominfo->set_room_name(info.roomName);
            roominfo->set_current_player_count(room->GetCurrentPlayerCount());
            roominfo->set_max_player_count(info.max_player_count);
        }
    }
}
