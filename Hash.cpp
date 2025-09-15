#include "Hash.h"
#include <sodium.h>
#include "iostream"

Hash::Hash()
{
    if (sodium_init() < 0) std::cerr << "libsodium 초기화 실패\n";
}

std::string Hash::Hashing(std::string password)
{
    char hashed_password[crypto_pwhash_STRBYTES];

    // Argon2i 알고리즘을 사용한 비밀번호 해싱
    if (crypto_pwhash_str(
            hashed_password,
            password.c_str(),
            password.length(),
            crypto_pwhash_OPSLIMIT_MODERATE, // 작업량 (강도)
            crypto_pwhash_MEMLIMIT_MODERATE) != 0)
    { // 메모리 사용량 (강도)
        std::cerr << "Argon2 해싱 실패\n";
        return std::string();
    }

    return std::string(hashed_password);
}

bool Hash::Compare(std::string DBpassword, std::string password)
{
    if (crypto_pwhash_str_verify(DBpassword.c_str(),
                             password.c_str(),
                             password.size()) == 0) return true;

    return false;
}
