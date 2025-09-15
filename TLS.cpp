#include "TLS.h"
#include "SslContextManager.h"
#include <iostream>
#include <openssl/err.h>

TLS::TLS()
{
    SSL *raw_ssl = SSL_new(SslContextManager::Instance().GetCTX());
    if (!raw_ssl)
    {
        std::cerr << "SSL_new failed\n";

        unsigned long err;
        while ((err = ERR_get_error()) != 0)
        {
            char buf[256];
            ERR_error_string_n(err, buf, sizeof(buf));
            std::cerr << "OpenSSL error: " << buf << "\n";
        }

        return;
    }

    rbio = BIO_new(BIO_s_mem());
    wbio = BIO_new(BIO_s_mem());

    constexpr int BUF_SIZE = 4096;

    ssl.reset(raw_ssl);
    // rbio, wbio는 SSL_set_bio가 소유권을 가져가므로 원시 포인터로 보관

    SSL_set_bio(ssl.get(), rbio, wbio);
    SSL_set_accept_state(ssl.get());
}

size_t TLS::GetRecvText(Buffer &buf, int &err)
{
    err = SSL_ERROR_NONE;

    while (buf.GetRemainingSize())
    {
        int n = SSL_read(ssl.get(), buf.GetWriteBuffer(), buf.GetRemainingSize());

        if (n > 0)
            buf.OnWrite(n);
        else
        {
            err = SSL_get_error(ssl.get(), n);

            break;
        }
    }

    return buf.GetStoredSize();
}

int TLS::GetSecretText(std::unique_ptr<SendBuffer> &buf)
{
    while (buf->GetRemainingSize())
    {
        int n = BIO_read(wbio, buf->GetWriteBuffer(), buf->GetRemainingSize());

        if (n > 0)
            buf->OnWrite(n);
        else
            break;
    }

    return buf->GetStoredSize();
}

void TLS::PrintSSLErr(const char *prefix)
{
    std::cerr << prefix << " : ";
    unsigned long e;
    while ((e = ERR_get_error()) != 0)
    {
        char buf[256];
        ERR_error_string_n(e, buf, sizeof(buf));
        std::cerr << buf << " | ";
    }
    std::cerr << "\n";
}