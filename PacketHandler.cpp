#include "PacketHandler.h"
#include <iostream>
#include "DatabaseManager.h"
#include "RoomManager.h"

void PacketHandler::Init()
{
    _packetTable.resize(19);

    Register(
        0,
        []() -> google::protobuf::Message *
        { return new UnrealEngineMessage::Ping(); },
        [](Session* session, const google::protobuf::Message &message)
        {
            // Pong 응답 예제
            UnrealEngineMessage::Pong pong;
            pong.set_timestamp(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());

            session->Send(pong);
        });

        Register(
        1,
        []() -> google::protobuf::Message *
        { return new UnrealEngineMessage::Pong(); },
        [](Session* session, const google::protobuf::Message &message)
        {});

    Register(
        2,
        []() -> google::protobuf::Message *
        { return new UnrealEngineMessage::SigninRequest(); },
        [](Session* session, const google::protobuf::Message &message)
        {
            const auto &pkt = static_cast<const UnrealEngineMessage::SigninRequest &>(message);

            UnrealEngineMessage::ErrorCode err;
            auto success = DatabaseManager::Instance().Login(pkt.user_id(), pkt.password(), err);

            UnrealEngineMessage::SigninResponse signinresponse;

            signinresponse.set_success(success);
            if (success) session->SuccessLogin(pkt.user_id());

            signinresponse.set_error_code(err);
            
            session->Send(signinresponse);
        });

        Register(
        3,
        []() -> google::protobuf::Message *
        { return new UnrealEngineMessage::SigninResponse(); },
        [](Session* session, const google::protobuf::Message &message)
        {});

    Register(
        4,
        []() -> google::protobuf::Message *
        { return new UnrealEngineMessage::SignupRequest(); },
        [](Session* session, const google::protobuf::Message &message)
        {
            const auto &pkt = static_cast<const UnrealEngineMessage::SignupRequest &>(message);

            UnrealEngineMessage::SignupResponse signupResponse;

            if (auto success = DatabaseManager::Instance().JoinMembership(pkt.user_id(), pkt.password()))
            {
                signupResponse.set_success(false);

                if (success == 1062)
                    signupResponse.set_error_code(UnrealEngineMessage::ErrorCode::USER_ALREADY_EXISTS);
                else
                    signupResponse.set_error_code(UnrealEngineMessage::ErrorCode::UNKNOWN);
            }
            else
            {
                signupResponse.set_success(true);
                signupResponse.set_error_code(UnrealEngineMessage::ErrorCode::NONE);
            }

            session->Send(signupResponse);
        });

        Register(
        5,
        []() -> google::protobuf::Message *
        { return new UnrealEngineMessage::SignupResponse(); },
        [](Session* session, const google::protobuf::Message &message)
        {});

        Register(
        6,
        []() -> google::protobuf::Message *
        { return new UnrealEngineMessage::RegisterRequest(); },
        [](Session* session, const google::protobuf::Message &message)
        {
            const auto &pkt = static_cast<const UnrealEngineMessage::RegisterRequest &>(message);

            DatabaseManager::Instance().UserNameRegister(pkt.user_name(),session->GetUserId());
            
            UnrealEngineMessage::RegisterResponse registerResponse;
            session->Send(registerResponse);
            
        });

        Register(
        7,
        []() -> google::protobuf::Message *
        { return new UnrealEngineMessage::RegisterResponse(); },
        [](Session* session, const google::protobuf::Message &message)
        {});

    Register(
        8,
        []() -> google::protobuf::Message *
        { return new UnrealEngineMessage::CreateRoomRequest(); },
        [](Session* session, const google::protobuf::Message &message)
        {
            const auto &pkt = static_cast<const UnrealEngineMessage::CreateRoomRequest &>(message);

            UnrealEngineMessage::JoinRoomResponse joinroomResponse;

            RoomManager::Instance().CreateRoom(joinroomResponse, pkt, session);

            session->Send(joinroomResponse);
        });

    Register(
        9,
        []() -> google::protobuf::Message *
        { return new UnrealEngineMessage::FindRoomRequest(); },
        [](Session* session, const google::protobuf::Message &message)
        {
            const auto &pkt = static_cast<const UnrealEngineMessage::FindRoomRequest &>(message);

            UnrealEngineMessage::FindRoomResponse findroomResponse;

            RoomManager::Instance().GetRoom(findroomResponse, pkt.room_name());

            session->Send(findroomResponse);
        });

        Register(
        10,
        []() -> google::protobuf::Message *
        { return new UnrealEngineMessage::FindRoomResponse(); },
        [](Session* session, const google::protobuf::Message &message)
        {});

    Register(
        11,
        []() -> google::protobuf::Message *
        { return new UnrealEngineMessage::JoinRoomRequest(); },
        [](Session* session, const google::protobuf::Message &message)
        {
            UnrealEngineMessage::JoinRoomResponse joinroomResponse;

            const auto &pkt = static_cast<const UnrealEngineMessage::JoinRoomRequest &>(message);

            if (auto room = RoomManager::Instance().GetRoom(pkt.id()))
            {
                if (auto success = room->Enter(joinroomResponse, session))
                {
                    joinroomResponse.set_success(true);

                    session->Send(joinroomResponse);

                    return;
                }
            }

            joinroomResponse.set_success(false);

            session->Send(joinroomResponse);
        });

        Register(
        12,
        []() -> google::protobuf::Message *
        { return new UnrealEngineMessage::JoinRoomResponse(); },
        [](Session* session, const google::protobuf::Message &message)
        {});

    Register(
        13,
        []() -> google::protobuf::Message *
        { return new UnrealEngineMessage::LeaveRoomRequest(); },
        [](Session* session, const google::protobuf::Message &message)
        {
            const auto &pkt = static_cast<const UnrealEngineMessage::LeaveRoomRequest &>(message);

            if (auto room = RoomManager::Instance().GetRoom(session->GetRoomId()))
            {
                room->Leave(session);
            }

            UnrealEngineMessage::FindRoomResponse findroomResponse;

            RoomManager::Instance().GetRoom(findroomResponse, pkt.room_name());

            session->Send(findroomResponse);
        });

        Register(
        14,
        []() -> google::protobuf::Message *
        { return new UnrealEngineMessage::UpdateRoomInfo(); },
        [](Session* session, const google::protobuf::Message &message)
        {});
        
    Register(
        15,
        []() -> google::protobuf::Message *
        { return new UnrealEngineMessage::BroadcastRequest(); },
        [](Session* session, const google::protobuf::Message &message)
        {
            const auto &pkt = static_cast<const UnrealEngineMessage::BroadcastRequest &>(message);

            if (auto room = RoomManager::Instance().GetRoom(session->GetRoomId()))
            {
                room->Broadcast(session, pkt.message());
            }
        });
        
        Register(
        16,
        []() -> google::protobuf::Message *
        { return new UnrealEngineMessage::BroadcastResponse(); },
        [](Session* session, const google::protobuf::Message &message)
        {});
        
        Register(
        17,
        []() -> google::protobuf::Message *
        { return new UnrealEngineMessage::GameRequest(); },
        [](Session* session, const google::protobuf::Message &message)
        {
            const auto &pkt = static_cast<const UnrealEngineMessage::GameRequest &>(message);

            if (auto room = RoomManager::Instance().GetRoom(session->GetRoomId()))
            {
                room->StartGame(pkt.url(),session);
            }
        });

        Register(
        18,
        []() -> google::protobuf::Message *
        { return new UnrealEngineMessage::GameResponse(); },
        [](Session* session, const google::protobuf::Message &message)
        {});
}

void PacketHandler::Register(uint16_t id, std::function<google::protobuf::Message *()> creator, PacketHandlerFunc handler)
{
    _packetTable[id] = std::make_unique<PacketEntry>(creator, handler);

    // 타입 이름을 가져오기 위해 임시로 메시지 생성
    std::unique_ptr<google::protobuf::Message> temp(creator());
    std::string typeName = temp->GetTypeName();
    _nameToId[typeName] = id;

    std::cout << "Registered message: " << typeName << " with ID: " << id << std::endl;
}

google::protobuf::Message *PacketHandler::CreateMessageById(uint16_t id)
{
    if (id >= _packetTable.size())
    {
        std::cerr << "CreateMessageById: Invalid ID (out of range): " << id << std::endl;
        return nullptr;
    }

    if (auto &creator = _packetTable[id]->_creator)
        return creator();

    std::cerr << "CreateMessageById: Unknown ID: " << id << std::endl;
    return nullptr;
}

uint16_t PacketHandler::GetMessageIdByName(const std::string &typeName)
{

    auto it = _nameToId.find(typeName);
    if (it != _nameToId.end())
    {
        return it->second;
    }
    std::cerr << "GetMessageIdByName: Unknown type name: " << typeName << std::endl;
    return 0;
}

void PacketHandler::HandlePacket(Session* session, uint16_t packetId, const google::protobuf::Message &message)
{

    if (packetId >= _packetTable.size())
    {
        std::cerr << "CreateMessageById: Invalid ID (out of range): " << packetId << std::endl;
        return;
    }

    if (auto &handler = _packetTable[packetId]->_handler)
        return handler(session, message);

    std::cerr << "HandlePacket: Unknown packet ID: " << packetId << std::endl;
}