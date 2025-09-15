#include "memory"
#include <openssl/ssl.h>
#include "SendBuffer.h"

struct SSLDeleter
{
    void operator()(SSL *ptr) const
    {
        if (ptr)
            SSL_free(ptr);
    }
};

class TLS
{
public:
    TLS();

    size_t GetRecvText(Buffer& buf,int& err);
    int GetSecretText(std::unique_ptr<SendBuffer>& buf);

    void PrintSSLErr(const char *prefix);

    SSL* GetSSL() {return ssl.get();}
    BIO* GetRBIO() {return rbio;}
    BIO* GetWBIO() {return wbio;}
    
    bool GetHandshaking() {return handshaking;}
    bool SetHandshaking(bool _handshaking) {return handshaking=_handshaking;}
private:
    std::unique_ptr<SSL, SSLDeleter> ssl;
    BIO* rbio;
    BIO* wbio;
    
    bool handshaking=true;

};