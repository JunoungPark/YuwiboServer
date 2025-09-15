#include "Listener.h"
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <liburing.h>
#include "Worker.h"

bool Listener::StartAccept(io_uring &ring, uint16_t port)
{
    struct io_uring_sqe *sqe_socket = io_uring_get_sqe(&ring);
    if (!sqe_socket)
        return false;

    io_uring_prep_socket_direct(sqe_socket, AF_INET, SOCK_STREAM, 0, 0, 0);
    sqe_socket->flags |= IOSQE_IO_LINK | SOCK_NONBLOCK | SOCK_CLOEXEC;
    // io_uring_prep_socket_direct(sqe_socket, AF_INET, SOCK_STREAM, 0, 0, SOCK_NONBLOCK | SOCK_CLOEXEC);

    io_uring_sqe_set_data(sqe_socket, nullptr);

    auto chain_context_uptr = std::make_unique<SocketSetupChainContext>(port);

    // fcntl(fd, F_SETFL, O_NONBLOCK);
    // fcntl(fd, F_SETFD, FD_CLOEXEC);
    // setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &chain_context_uptr->setsockopt_opt_val, sizeof(chain_context_uptr->setsockopt_opt_val));

    struct io_uring_sqe *sqe_setsockopt = io_uring_get_sqe(&ring);
    if (!sqe_setsockopt)
        return false;

    io_uring_prep_cmd_sock(sqe_setsockopt, SOCKET_URING_OP_SETSOCKOPT, 0, SOL_SOCKET, SO_REUSEADDR, &chain_context_uptr->setsockopt_opt_val, sizeof(chain_context_uptr->setsockopt_opt_val));
    sqe_setsockopt->flags |= IOSQE_IO_LINK | IOSQE_FIXED_FILE;

    io_uring_sqe_set_data(sqe_setsockopt, nullptr);

    io_uring_sqe *sqe_bind = io_uring_get_sqe(&ring);
    if (!sqe_bind)
        return false;

    io_uring_prep_bind(sqe_bind, 0, (sockaddr *)&chain_context_uptr->bind_addr, sizeof(chain_context_uptr->bind_addr));
    sqe_bind->flags |= IOSQE_IO_LINK | IOSQE_FIXED_FILE;

    io_uring_sqe_set_data(sqe_bind, nullptr);

    io_uring_sqe *sqe_listen = io_uring_get_sqe(&ring);
    if (!sqe_listen)
        return false;

    io_uring_prep_listen(sqe_listen, 0, SOMAXCONN);
    sqe_listen->flags |= IOSQE_IO_LINK | IOSQE_FIXED_FILE;

    io_uring_sqe_set_data(sqe_listen, chain_context_uptr.release());

    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    if (!sqe)
        return false;

    io_uring_prep_multishot_accept(sqe, 0, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
    sqe->flags |=  IOSQE_FIXED_FILE;
    
    accept_ptr = std::make_unique<RequestContext>(REQ_TYPE_ACCEPT);
    io_uring_sqe_set_data(sqe, accept_ptr.get());
    // io_uring_prep_multishot_accept(sqe, 0 , (sockaddr*)&client_context_uptr->client_addr_buf, &client_context_uptr->client_addr_len, SOCK_NONBLOCK | SOCK_CLOEXEC);

    io_uring_submit(&ring);

    return true;
}

void Listener::StartStop(io_uring &ring)
{
    if (auto sqe_cancel = io_uring_get_sqe(&ring))
    {
        io_uring_prep_cancel(sqe_cancel, accept_ptr.release(), 0);
        io_uring_sqe_set_flags(sqe_cancel, IOSQE_IO_HARDLINK | IOSQE_CQE_SKIP_SUCCESS);

        auto context = std::make_unique<RequestContext>(REQ_TYPE_ACCEPT_CANCEL);
        io_uring_sqe_set_data(sqe_cancel, context.release());
    }

    if (auto sqe = io_uring_get_sqe(&ring))
    {
        io_uring_prep_close_direct(sqe, 0);
        auto context = std::make_unique<RequestContext>(REQ_TYPE_LISTENER_CLOSE);
        io_uring_sqe_set_data(sqe, context.release());
    }

    io_uring_submit(&ring);
}
