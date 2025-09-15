#include "Session.h"
#include <iostream>
#include <unistd.h>
#include "PacketHandler.h"
#include <openssl/err.h>
#include "SslContextManager.h"
#include "Worker.h"

Session::Session(int fd)
    : _fd(fd)
{
}

void Session::Strat()
{
    StartRecv();
}

void Session::StartRecv()
{
    auto sqe = io_uring_get_sqe(&worker->GetRing());
    io_uring_prep_recv_multishot(sqe, _fd, nullptr, 0, 0);
    sqe->flags |= IOSQE_BUFFER_SELECT;
    sqe->buf_group = worker->GetBufGroupId();

    // io_uring_prep_recv_multishot(sqe, _fd, _recvBuffer.GetWriteBuffer(), _recvBuffer.GetFreeSize(), 0);
    session_recv_ptr = std::make_unique<SessionContext>(this, REQ_TYPE_RECV);
    io_uring_sqe_set_data(sqe, session_recv_ptr.get());
    io_uring_submit(&worker->GetRing());
}

void Session::Send(const std::string &data)
{
    auto sqe = io_uring_get_sqe(&worker->GetRing());
    char *buffer = new char[data.size()];
    memcpy(buffer, data.data(), data.size());
    io_uring_prep_send(sqe, _fd, buffer, data.size(), 0);
    io_uring_sqe_set_data(sqe, buffer);
    io_uring_submit(&worker->GetRing());

    SendCount++;
}

void Session::Send(const google::protobuf::Message &message)
{
    std::string serialized = message.SerializeAsString();
    uint16_t msgId = htons(PacketHandler::Instance().GetMessageIdByName(message.GetTypeName()));
    uint16_t packetLen = htons(sizeof(msgId) + serialized.size());

    Buffer buffer(sizeof(packetLen) + sizeof(msgId) + serialized.size());
    buffer.Write(reinterpret_cast<const char *>(&packetLen), sizeof(packetLen));
    buffer.Write(reinterpret_cast<const char *>(&msgId), sizeof(msgId));
    buffer.Write(serialized.data(), serialized.size());

    int wret = SSL_write(_tls.GetSSL(), buffer.GetBuffer(), buffer.GetStoredSize());
    if (wret <= 0)
    {
        int err = SSL_get_error(_tls.GetSSL(), wret);
        if (err == SSL_ERROR_WANT_WRITE || err == SSL_ERROR_WANT_READ)
            ;
        else
        {
            std::cerr << "SSL_write failed" << " : ";
            unsigned long e;
            while ((e = ERR_get_error()) != 0)
            {
                char buf[256];
                ERR_error_string_n(e, buf, sizeof(buf));
                std::cerr << buf << " | ";
            }
            std::cerr << "\n";
            Close(false);

            return;
        }
    }

    FlushTLSWriteBuffer();
}

void Session::HandlePacket(const char *data, size_t len)
{
    uint16_t msgId;
    memcpy(&msgId, data, sizeof(msgId));
    msgId = ntohs(msgId);

    std::unique_ptr<google::protobuf::Message> message(PacketHandler::Instance().CreateMessageById(msgId));
    if (!message)
        return;

    if (!message->ParseFromArray(data + sizeof(msgId), len - sizeof(msgId)))
        return;

    std::cout<< "Handle Packet message Type : " << message->GetTypeName() <<std::endl;

    PacketHandler::Instance().HandlePacket(this, msgId, *message.get());
}

void Session::OnTLSHandshakeComplete()
{
    _tls.SetHandshaking(false);
    std::cout << "Handshake complete for fd=" << _fd << "\n";
}

void Session::FlushTLSWriteBuffer()
{
    auto buf = std::make_unique<SendBuffer>();
    auto n = _tls.GetSecretText(buf);

    if (n <= 0)
        return;

    _sendQueue.push(std::move(buf));

    if (!_isSending)
    {
        _isSending = true;
        SendInternal();
    }
}

bool Session::TLSHandshake()
{
    std::cout << "TLS Handshake\n";

    int ret = SSL_do_handshake(_tls.GetSSL());

    FlushTLSWriteBuffer();

    if (ret == 1)
    {
        // 핸드셰이크 성공
        OnTLSHandshakeComplete();
        return true;
    }

    int err = SSL_get_error(_tls.GetSSL(), ret);
    
    if (err == SSL_ERROR_WANT_READ);
    else if (err == SSL_ERROR_WANT_WRITE)
        FlushTLSWriteBuffer();
    else
    {
        std::cerr << "Handshake failed\n";
        ERR_print_errors_fp(stderr);
        Close(false);
    }

    return false;
}

void Session::SendInternal()
{
    if (_sendQueue.empty())
        return;

    auto &buffer = _sendQueue.front();
    size_t len = buffer->GetStoredSize();

    auto sqe = io_uring_get_sqe(&worker->GetRing());
    if (!sqe)
        return;

    auto context = std::make_unique<SessionSendContext>(this, buffer->GetReadBuffer(), len, REQ_TYPE_SEND);

    io_uring_prep_send(sqe, _fd, context->data.get(), len, 0);
    io_uring_sqe_set_data(sqe, context.release());

    // 다음 SendInternal()은 OnSend()에서 호출
    io_uring_submit(&worker->GetRing());

    std::cout << "Send\n";

    SendCount++;
}

bool Session::ProcessPacket(Buffer &buffer)
{
    size_t offset = 0;
    const char *data = buffer.GetReadBuffer();
    size_t totalLen = buffer.GetStoredSize();

    while (offset + sizeof(uint16_t) <= totalLen)
    {
        uint16_t packetLen = 0;
        memcpy(&packetLen, data + offset, sizeof(uint16_t));
        packetLen = ntohs(packetLen);

        if (offset + sizeof(uint16_t) + packetLen > totalLen)
            break; // incomplete packet
    
        std::cout << "packet Len : " << packetLen << std::endl;

        const char *packetStart = data + offset + sizeof(uint16_t);
        HandlePacket(packetStart, packetLen);
        offset += sizeof(uint16_t) + packetLen;
    }

    if (offset < totalLen)
    {
        buffer.PreserveFrom(offset);
        return false;
    }

    buffer.Reset();
    return true;
}

void Session::StartClose(bool submit)
{
    if (canceling)
        return;

    auto sqe_cancel = io_uring_get_sqe(&worker->GetRing());
    if (!sqe_cancel)
        return;

    io_uring_prep_cancel(sqe_cancel, session_recv_ptr.get(), 0);

    auto context = std::make_unique<SessionContext>(this, REQ_TYPE_RECV_CANCEL);
    io_uring_sqe_set_data(sqe_cancel, context.release());
    // 이 요청을 다른 것과 체인하지 않습니다.
    std::cout << "Session " << _fd << ": recv_multishot cancel request prepared." << std::endl;

    if (submit)
        io_uring_submit(&worker->GetRing());

    canceling = true;
}

void Session::Close(bool AdditionalCheck)
{
    if (AdditionalCheck)
    {
        if (!canceling)
            return;

        if (_isSending)
            return;

        if (SendCount > 0)
            return;
    }

    if (closing)
        return;

    closing = true;

    auto sqe = io_uring_get_sqe(&worker->GetRing());
    if (!sqe)
        return;

    io_uring_prep_close(sqe, _fd);
    auto context = std::make_unique<SessionCloseContext>(_fd);
    io_uring_sqe_set_data(sqe, context.release());

    io_uring_submit(&worker->GetRing());
}

void Session::OnRecv(io_uring_cqe &cqe)
{
    if (cqe.res <= 0)
    {
        Close(false);
        return;
    }

    unsigned short bufId = cqe.flags >> IORING_CQE_BUFFER_SHIFT;
    auto recvBuf = worker->GetRecvBuffer(bufId);

    recvBuf->OnWrite(cqe.res);

    std::cout << "cqe res : " << recvBuf->GetStoredSize() << std::endl;

    while (recvBuf->GetStoredSize())
    {
        int written = BIO_write(_tls.GetRBIO(), recvBuf->GetReadBuffer(), recvBuf->GetStoredSize());

        if (written <= 0)
        {
            _tls.PrintSSLErr("BIO_write failed");
            Close(false);
            return;
        }
        else
            recvBuf->OnRead(written);
    }

    if (_tls.GetHandshaking())
    {
        if (!TLSHandshake())
        {
            recvBuf->Reset();

            io_uring_buf_ring_add(worker->GetBufRing(), recvBuf->GetBuffer(), recvBuf->GetBufferSize(), bufId, worker->GetBufCount() - 1, 0);
            io_uring_buf_ring_advance(worker->GetBufRing(), 1);

            return;
        }
    }

    Buffer buf;

    int err;
    int n = _tls.GetRecvText(buf, err);

    if (err == SSL_ERROR_NONE || err == SSL_ERROR_WANT_READ)
        ;
    else if (err == SSL_ERROR_WANT_WRITE)
    {
        // wbio에 뭔가 쌓였을 수 있음 -> flush
        FlushTLSWriteBuffer();
    }
    else
    {
        _tls.PrintSSLErr("SSL_read error");
        Close(false);
    }
    // 핸드셰이크가 끝났다면 application data 처리
    
    std::cout << "Recv Text : " << n << " err : " << err << std::endl;

    if (n > 0)
    {
        if (recvBuf->GetBufferSize() < n)
            recvBuf->Resize(n);
        else
            recvBuf->Reset();

        recvBuf->Write(buf.GetBuffer(), n);

        if (_pendingRecvBufId != -1)
        {
            // 이전에 잘린 패킷이 있으면 이어붙임
            auto _pendingRecvBuffer = worker->GetRecvBuffer(_pendingRecvBufId);

            _pendingRecvBuffer->Append(*recvBuf);
            // Append = 뒤에 붙이는 함수 (추가 구현 필요)
            recvBuf->Reset(); // 이제 이 버퍼는 끝났음
            io_uring_buf_ring_add(worker->GetBufRing(), recvBuf->GetBuffer(), recvBuf->GetBufferSize(), bufId, worker->GetBufCount() - 1, 0);
            io_uring_buf_ring_advance(worker->GetBufRing(), 1);

            if (ProcessPacket(*_pendingRecvBuffer))
            {
                _pendingRecvBuffer->Reset();

                io_uring_buf_ring_add(worker->GetBufRing(), _pendingRecvBuffer->GetBuffer(), _pendingRecvBuffer->GetBufferSize(), _pendingRecvBufId, worker->GetBufCount() - 1, 0);

                io_uring_buf_ring_advance(worker->GetBufRing(), 1);
                _pendingRecvBufId = -1; // 끝까지 처리됨
            }
            // else: 여전히 잘림 상태, 다음 수신 대기
        }
        else
        {
            if (!ProcessPacket(*recvBuf))
            {
                _pendingRecvBufId = bufId; // 잘린 데이터 보존
            }
            else
            {
                recvBuf->Reset();

                io_uring_buf_ring_add(worker->GetBufRing(), recvBuf->GetBuffer(), recvBuf->GetBufferSize(), bufId, worker->GetBufCount() - 1, 0);
                io_uring_buf_ring_advance(worker->GetBufRing(), 1);
            }
        }
    }
}

void Session::OnSend(ssize_t bytes)
{
    SendCount--;

    if (bytes <= 0)
    {
        Close(false);
        return;
    }

    if (!_sendQueue.empty())
    {
        auto &completed = _sendQueue.front();
        completed->OnRead(bytes);

        if (completed->GetStoredSize() == 0)
        {
            _sendQueue.pop();
            if (_sendQueue.empty())
            {
                _isSending = false;
            }
        }
    }

    // 다음 전송
    SendInternal();

    Close(true);
}