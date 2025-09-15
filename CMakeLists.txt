cmake_minimum_required(VERSION 3.10.0)
project(
    onlinesystem
 VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_TOOLCHAIN_FILE "${HOME}/vcpkg/scripts/buildsystems/vcpkg.cmake")
set(CMAKE_PREFIX_PATH "$ENV{HOME}/vcpkg/installed/x64-linux")
# Protobuf 모듈 찾기
find_package(Protobuf CONFIG REQUIRED)
find_package(OpenSSL REQUIRED) 
find_package(PkgConfig REQUIRED)
pkg_check_modules(SODIUM REQUIRED libsodium)
# Protobuf 컴파일
# add_protobuf_sources() 매크로는 .proto 파일을 컴파일하여
# 자동으로 소스 목록에 추가해 줍니다.
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS UnrealEngineMessage.proto)

# 실행 파일 빌드
add_executable(
    onlinesystem 
Buffer.cpp DatabaseManager.cpp Listener.cpp RecvBuffer.cpp Room.cpp RoomManager.cpp SendBuffer.cpp Session.cpp SessionManager.cpp SslContextManager.cpp ThreadManager.cpp main.cpp TLS.cpp Hash.cpp PacketHandler.cpp RequestContext.h Worker.cpp ${PROTO_SRCS})

# 새로운 추가: 프로토버퍼 헤더 파일 경로를 인클루드 디렉토리에 추가
target_include_directories(
    onlinesystem
    PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${Protobuf_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR} # .proto 파일이 현재 디렉토리에 있다면 이 경로도 포함
)

target_link_directories(
    onlinesystem 
    PRIVATE 
    /usr/lib64/mysql
    /usr/lib64/libssl
    ${SODIUM_INCLUDE_DIRS}
)

# Protobuf 라이브러리 연결
target_link_libraries(
    onlinesystem
    PRIVATE
    mysqlclient 
    uring
    protobuf::libprotobuf
    OpenSSL::SSL  
    OpenSSL::Crypto
    ${SODIUM_LIBRARIES}
)