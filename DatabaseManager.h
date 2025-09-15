#pragma once
#include <mutex>
#include <string>
#include <mysql/mysql.h>
#include "Hash.h"
#include "UnrealEngineMessage.pb.h"
struct UserInfo{

    bool success=false;
    std::string user_name;   
};

class DatabaseManager
{

private:
    DatabaseManager() = default; // 기본 생성자 private
public:
    ~DatabaseManager() { Disconnect(); }

    DatabaseManager(const DatabaseManager &) = delete;            // 복사 방지
    DatabaseManager &operator=(const DatabaseManager &) = delete; // 대입 방지

    inline static DatabaseManager &Instance()
    {
        static DatabaseManager instance;
        return instance;
    }

    inline MYSQL *GetConnection()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return _conn;
    }

    bool Connect(const std::string &host, const std::string &user, const std::string &password, const std::string &db, unsigned int port = 3306);
    void CreateSTMT();
    bool Login(std::string user_id, std::string password, UnrealEngineMessage::ErrorCode& err);
    uint32_t JoinMembership(std::string user_id, std::string password);
    bool UserNameRegister(std::string user_name, std::string user_id);
    UserInfo GetUserInfo(std::string user_id);
    
    void Disconnect();

private:
    MYSQL *_conn = nullptr;
    MYSQL_STMT *loginstmt;
    MYSQL_STMT *joinmembershipstmt;
    MYSQL_STMT *usernamestmt;
    MYSQL_STMT *userinfostmt;
    
    Hash hash;

    std::mutex _mutex;
};