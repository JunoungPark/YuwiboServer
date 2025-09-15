#pragma once
#include <memory>
#include <openssl/ssl.h>

struct CTXDeleter
{
    void operator()(SSL_CTX *ptr) const
    {
        if (ptr)
            SSL_CTX_free(ptr);
    }
};

class SslContextManager {
private:
    SslContextManager() = default;
public:
    ~SslContextManager() {}

    SslContextManager(const SslContextManager&) = delete;      // 복사 방지
    SslContextManager& operator=(const SslContextManager&) = delete; // 대입 방지

    inline static SslContextManager& Instance(){
        static SslContextManager instance;
        return instance;
    }

    bool Init();

    SSL_CTX* GetCTX() {return ctx.get();}
private:

    std::unique_ptr<SSL_CTX,CTXDeleter> ctx;
};