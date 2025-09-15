#pragma once
#include <unordered_map>
#include <memory>
#include "Room.h"
#include "UnrealEngineMessage.pb.h"

class RoomManager {
private:
    RoomManager() = default;
public:

    RoomManager(const RoomManager&) = delete;      // 복사 방지
    RoomManager& operator=(const RoomManager&) = delete; // 대입 방지

    inline static RoomManager& Instance(){
        static RoomManager instance;
        return instance;
    }
    
    void CreateRoom(UnrealEngineMessage::JoinRoomResponse &joinroomResponse, const UnrealEngineMessage::CreateRoomRequest &message, Session* session);
    void RemoveRoom(int roomId);

    Room* GetRoom(int roomId);
    void GetRoom(UnrealEngineMessage::FindRoomResponse& findroomResponse,std::string roomName);
private:
    std::unordered_map<uint, std::unique_ptr<Room>> _rooms;
 
    mutable std::mutex _mutex;
};
