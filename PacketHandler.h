#pragma once
#include <unordered_map>
#include <functional>
#include <memory>
#include "Session.h"
#include "UnrealEngineMessage.pb.h"

using PacketHandlerFunc = std::function<void(Session*, const google::protobuf::Message&)>;

struct PacketEntry{

    PacketEntry(std::function<google::protobuf::Message*()> creator,PacketHandlerFunc handler) : _creator(creator),_handler(handler) {}

    std::function<google::protobuf::Message*()> _creator;
    PacketHandlerFunc _handler;

};

class PacketHandler {
private:
PacketHandler() = default;
public:

    PacketHandler(const PacketHandler&) = delete;      // 복사 방지
    PacketHandler& operator=(const PacketHandler&) = delete; // 대입 방지
    
    inline static PacketHandler& Instance() {
        static PacketHandler instance;
        return instance;
    }

    void Init();
     void Register(uint16_t id, std::function<google::protobuf::Message*()> creator, PacketHandlerFunc handler);

    // 메시지 ID로 메시지 생성
    google::protobuf::Message* CreateMessageById(uint16_t id);

    // 타입 이름으로 메시지 ID 반환
    uint16_t GetMessageIdByName(const std::string& typeName);
    void HandlePacket(Session* session, uint16_t packetId, const google::protobuf::Message& message);

private:
    std::vector<std::unique_ptr<PacketEntry>> _packetTable;
    std::unordered_map<std::string, uint16_t> _nameToId;

};