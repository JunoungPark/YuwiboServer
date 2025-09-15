#include "SslContextManager.h"
#include <iostream>
#include <openssl/err.h>

bool SslContextManager::Init()
{
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    SSL_CTX *ctx_ptr = SSL_CTX_new(TLS_server_method());
    if (!ctx_ptr)
    {
        std::cerr << "SSL_CTX_new failed\n";
        return false;
    }

    ctx.reset(ctx_ptr);

    if (SSL_CTX_use_certificate_file(ctx.get(), "/etc/ssl/server.crt", SSL_FILETYPE_PEM) <= 0)
    {
        std::cerr << "SSL_CTX_use_certificate_file failed\n";
        ERR_print_errors_fp(stderr);
        return false;
    }

    if (SSL_CTX_use_PrivateKey_file(ctx.get(), "/etc/ssl/server.key", SSL_FILETYPE_PEM) <= 0)
    {
        std::cerr << "SSL_CTX_use_PrivateKey_file failed\n";
        ERR_print_errors_fp(stderr);
        return false;
    }

    return true;
}