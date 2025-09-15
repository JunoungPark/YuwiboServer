#include "DatabaseManager.h"
#include <iostream>
#include <string.h>
#include "sodium.h"
bool DatabaseManager::Connect(const std::string &host, const std::string &user, const std::string &password, const std::string &db, unsigned int port)
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_conn = mysql_init(nullptr))
    {
        if (!mysql_real_connect(_conn, host.c_str(), user.c_str(), password.c_str(), db.c_str(), port, nullptr, 0))
        {
            std::cerr << "mysql_real_connect failed: " << mysql_error(_conn) << std::endl;
            mysql_close(_conn);
            _conn = nullptr;
            return false;
        }
        CreateSTMT();
        return true;
    }

    std::cerr << "mysql_init failed" << std::endl;
    return false;
}

void DatabaseManager::CreateSTMT()
{
    loginstmt = mysql_stmt_init(_conn);
    if (!loginstmt)
    {
        fprintf(stderr, "mysql_stmt_init() failed\n");
        return;
    }

    const char *loginquery = "SELECT password, name FROM user WHERE id = ?";

    if (mysql_stmt_prepare(loginstmt, loginquery, strlen(loginquery)))
    {
        fprintf(stderr, "mysql_stmt_prepare() failed: %s\n", mysql_stmt_error(loginstmt));
        return;
    }

    joinmembershipstmt = mysql_stmt_init(_conn);
    if (!joinmembershipstmt)
    {
        fprintf(stderr, "mysql_stmt_init() failed\n");
        return;
    }

    const char *joinmembershipstmtquery = "INSERT INTO user (id, password) VALUES (?, ?)";

    if (mysql_stmt_prepare(joinmembershipstmt, joinmembershipstmtquery, strlen(joinmembershipstmtquery)))
    {
        fprintf(stderr, "mysql_stmt_prepare() failed: %s\n", mysql_stmt_error(joinmembershipstmt));
        return;
    }

    usernamestmt= mysql_stmt_init(_conn);
    if (!usernamestmt)
    {
        fprintf(stderr, "mysql_stmt_init() failed\n");
        return;
    }

    const char *usernamequery = "UPDATE user SET name = ? WHERE id = ?";

    if (mysql_stmt_prepare(usernamestmt, usernamequery, strlen(usernamequery)))
    {
        fprintf(stderr, "mysql_stmt_prepare() failed: %s\n", mysql_stmt_error(usernamestmt));
        return;
    }

    userinfostmt= mysql_stmt_init(_conn);
    if (!userinfostmt)
    {
        fprintf(stderr, "mysql_stmt_init() failed\n");
        return;
    }

    const char *userinfoquery = "SELECT name FROM user WHERE id = ?";

    if (mysql_stmt_prepare(userinfostmt, userinfoquery, strlen(userinfoquery)))
    {
        fprintf(stderr, "mysql_stmt_prepare() failed: %s\n", mysql_stmt_error(userinfostmt));
        return;
    }
}

bool DatabaseManager::Login(std::string user_id, std::string password, UnrealEngineMessage::ErrorCode& err)
{
    std::lock_guard<std::mutex> lock(_mutex);

    MYSQL_BIND bind;

    memset(&bind, 0, sizeof(bind));

    const char *c_user_id = user_id.c_str();

    unsigned long id_len = user_id.length();

    // 첫 번째 파라미터 (int) 바인딩
    bind.buffer_type = MYSQL_TYPE_VARCHAR;
    bind.buffer = (void*)c_user_id;
    bind.buffer_length = id_len;
    bind.is_null = 0;
    bind.length = &id_len;

    if (mysql_stmt_bind_param(loginstmt, &bind))
    {
        fprintf(stderr, "mysql_stmt_bind_param() failed: %s\n", mysql_stmt_error(loginstmt));
        err = UnrealEngineMessage::ErrorCode::UNKNOWN;
        return false;
    }

    if (mysql_stmt_execute(loginstmt))
    {
        fprintf(stderr, "mysql_stmt_execute() failed: %s\n", mysql_stmt_error(loginstmt));
        err = UnrealEngineMessage::ErrorCode::UNKNOWN;
        return false;
    }

    MYSQL_BIND res_bind[2];
    memset(res_bind, 0, sizeof(res_bind));

    // 2. 결과를 저장할 변수 선언

    char password_buffer[crypto_pwhash_STRBYTES];
    char name_buffer[50];
    bool is_name_null = false;

    // 3. 결과 컬럼과 바인딩 구조체 연결
    res_bind[0].buffer_type = MYSQL_TYPE_STRING;
    res_bind[0].buffer = password_buffer;
    res_bind[0].buffer_length = sizeof(password_buffer);

    res_bind[1].buffer_type = MYSQL_TYPE_STRING;
    res_bind[1].buffer = name_buffer; // name값 자체는 필요 없으므로 버퍼를 비움
    res_bind[1].buffer_length = sizeof(name_buffer);
    res_bind[1].is_null = &is_name_null;

    // 4. 바인딩된 결과 컬럼을 준비된 문과 연결
    if (mysql_stmt_bind_result(loginstmt, res_bind))
    {
        fprintf(stderr, "mysql_stmt_bind_result() failed: %s\n", mysql_stmt_error(loginstmt));
        err = UnrealEngineMessage::ErrorCode::UNKNOWN;
    }
    int status = mysql_stmt_fetch(loginstmt);
 
    // 6. 결과가 있을 경우 처리
    if (status != 0) {
        // 결과가 없거나 오류 발생
        fprintf(stderr, "mysql_stmt_fetch() failed: %s\n", mysql_stmt_error(loginstmt));
        err = UnrealEngineMessage::ErrorCode::INVALID_REQUEST;
        return false;
    }

    
    // 7. 자원 해제 (필요한 경우)
    mysql_stmt_free_result(loginstmt);
    mysql_stmt_reset(loginstmt);
    // 8. 로그인 성공 여부 반환
    auto success = hash.Compare(std::string(password_buffer),password);
    if(success)
    {
        if (is_name_null) err = UnrealEngineMessage::ErrorCode::USER_NAME_NONE;
        else err = UnrealEngineMessage::ErrorCode::NONE;
    }
    else err = UnrealEngineMessage::ErrorCode::INVALID_REQUEST;

    return success;
    
}

uint32_t DatabaseManager::JoinMembership(std::string user_id, std::string password)
{
    std::lock_guard<std::mutex> lock(_mutex);

    MYSQL_BIND bind[2];

    memset(bind, 0, sizeof(bind));

    const char *c_user_id = user_id.c_str();

    auto hash_password = hash.Hashing(password);
    if(!hash_password.length()) return -1;

    const char *c_password = hash_password.c_str();

    unsigned long id_len = user_id.length();
    unsigned long password_len = hash_password.length();

    // 첫 번째 파라미터 (int) 바인딩
    bind[0].buffer_type = MYSQL_TYPE_VARCHAR;
    bind[0].buffer = (void*)c_user_id;
    bind[0].buffer_length = id_len;
    bind[0].is_null = 0;
    bind[0].length = &id_len;

    // 두 번째 파라미터 (string) 바인딩
    bind[1].buffer_type = MYSQL_TYPE_VARCHAR;
    bind[1].buffer = (void*)c_password;
    bind[1].buffer_length = password_len; // 버퍼의 크기
    bind[1].is_null = 0;
    bind[1].length = &password_len; // 실제 데이터의 길이
    
    if (mysql_stmt_bind_param(joinmembershipstmt, bind))
    {
        fprintf(stderr, "mysql_stmt_bind_param() failed: %s\n", mysql_stmt_error(joinmembershipstmt));
        return -1;
    }

    if (mysql_stmt_execute(joinmembershipstmt))
    {
        int error_no = mysql_stmt_errno(joinmembershipstmt);

        if (error_no == 1062)
        {
            fprintf(stderr, "회원가입 실패: 이미 존재하는 사용자 ID입니다.\n");
        }
        else
        {
            fprintf(stderr, "회원가입 실패: %s (Error code: %d)\n", mysql_stmt_error(joinmembershipstmt), error_no);
        }
        return error_no;
    }

    return 0;
}

bool DatabaseManager::UserNameRegister(std::string user_name, std::string user_id)
{
    std::lock_guard<std::mutex> lock(_mutex);

    MYSQL_BIND bind[2];

    memset(bind, 0, sizeof(bind));

    const char *c_user_name = user_name.c_str();
    const char *c_user_id = user_id.c_str();

    unsigned long name_len = user_name.length();
    unsigned long id_len = user_id.length();

    // 첫 번째 파라미터 (int) 바인딩
    bind[0].buffer_type = MYSQL_TYPE_VARCHAR;
    bind[0].buffer = (void*)c_user_name;
    bind[0].buffer_length = name_len; // 버퍼의 크기
    bind[0].is_null = 0;
    bind[0].length = &name_len; // 실제 데이터의 길이

    // 두 번째 파라미터 (string) 바인딩
    bind[1].buffer_type = MYSQL_TYPE_VARCHAR;
    bind[1].buffer = (void*)c_user_id;
    bind[1].buffer_length = id_len;
    bind[1].is_null = 0;
    bind[1].length = &id_len;

    if (mysql_stmt_bind_param(usernamestmt, bind)) {
        fprintf(stderr, "mysql_stmt_bind_param() failed: %s\n", mysql_stmt_error(usernamestmt));
        return false; // 실패 시 false 반환
    }

    if (mysql_stmt_execute(usernamestmt)) {
        int error_no = mysql_stmt_errno(usernamestmt);
        fprintf(stderr, "닉네임 등록 실패: %s (Error code: %d)\n", mysql_stmt_error(usernamestmt), error_no);
        return false; // 실패 시 false 반환
    }

    // 성공적으로 닉네임이 업데이트되었는지 확인
    my_ulonglong affected_rows = mysql_stmt_affected_rows(usernamestmt);
    if (affected_rows == 0) {
        // user_id가 존재하지 않아 업데이트된 행이 없는 경우
        fprintf(stderr, "닉네임 등록 실패: 해당 user_id가 존재하지 않습니다.\n");
        return false;
    }

    return true;
}

UserInfo DatabaseManager::GetUserInfo(std::string user_id)
{
    std::lock_guard<std::mutex> lock(_mutex);

    MYSQL_BIND bind;
   
    memset(&bind, 0, sizeof(bind));

    const char *c_user_id = user_id.c_str();

    unsigned long id_len = user_id.length();

    // 첫 번째 파라미터 (int) 바인딩
    bind.buffer_type = MYSQL_TYPE_VARCHAR;
    bind.buffer = (void*)c_user_id;
    bind.buffer_length = id_len;
    bind.is_null = 0;
    bind.length = &id_len;
    
    UserInfo info;

    if (mysql_stmt_bind_param(userinfostmt, &bind))
    {
        fprintf(stderr, "mysql_stmt_bind_param() failed: %s\n", mysql_stmt_error(userinfostmt));
        return info;
    }

    if (mysql_stmt_execute(userinfostmt))
    {
        fprintf(stderr, "mysql_stmt_execute() failed: %s\n", mysql_stmt_error(userinfostmt));
        return info;
    }

    MYSQL_BIND res_bind;
    memset(&res_bind, 0, sizeof(res_bind));

    // 2. 결과를 저장할 변수 선언

    unsigned long name_len; // 문자열 길이를 저장할 변수

    char name_buffer[50];

    // 3. 결과 컬럼과 바인딩 구조체 연결
    res_bind.buffer_type = MYSQL_TYPE_STRING;
    res_bind.buffer = name_buffer;
    res_bind.buffer_length = sizeof(name_buffer);
    res_bind.length = &name_len;
    res_bind.is_null = 0;

    // 4. 바인딩된 결과 컬럼을 준비된 문과 연결
    if (mysql_stmt_bind_result(userinfostmt, &res_bind))
    {
        fprintf(stderr, "mysql_stmt_bind_result() failed: %s\n", mysql_stmt_error(userinfostmt));
        return info;
    }
    int status = mysql_stmt_fetch(userinfostmt);
 
    // 6. 결과가 있을 경우 처리
    if (status == 0) { // 성공적으로 가져온 경우
        info.success=true;
        // userInfo.name에 'name' 값이 저장됨
        info.user_name = std::string(name_buffer, name_len);
        
        printf("User found: %s\n", info.user_name.c_str()); 
    } else {
        // 결과가 없거나 오류 발생
        fprintf(stderr, "mysql_stmt_fetch() failed: %s\n", mysql_stmt_error(userinfostmt));
    }

    // 7. 자원 해제 (필요한 경우)
    mysql_stmt_free_result(userinfostmt);
    mysql_stmt_reset(userinfostmt);

    return info;
}

void DatabaseManager::Disconnect()
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_conn)
    {
        mysql_stmt_close(loginstmt);
        mysql_stmt_close(joinmembershipstmt);
        mysql_stmt_close(usernamestmt);
        mysql_stmt_close(userinfostmt);
        mysql_close(_conn); // MySQL 연결 종료
        _conn = nullptr;
        std::cout << "Database disconnected successfully." << std::endl;
    }
}