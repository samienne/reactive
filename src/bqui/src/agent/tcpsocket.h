#pragma once

// The one place the platform socket API is normalised. Both tcptransport.cpp
// and tcplistener.cpp include this so the `#ifdef _WIN32` split lives here and
// nowhere else. Everything defined here is inline (no linkage conflicts across
// the two translation units) and the header is private to the agent layer.

#ifdef _WIN32
#   ifndef WIN32_LEAN_AND_MEAN
#      define WIN32_LEAN_AND_MEAN
#   endif
#   include <winsock2.h>
#   include <ws2tcpip.h>
#else
#   include <cerrno>

#   include <arpa/inet.h>
#   include <netinet/in.h>
#   include <sys/socket.h>
#   include <unistd.h>
#endif

namespace bqui::agent
{
#ifdef _WIN32
    using RawSocket = SOCKET;
    inline constexpr RawSocket kInvalidSocket = INVALID_SOCKET;
    inline void closeRawSocket(RawSocket sock) { ::closesocket(sock); }
    // Wake a blocked recv() in another thread without freeing the socket, so
    // the reader gets an immediate EOF and the fd stays valid until close.
    inline void shutdownRawSocket(RawSocket sock) { ::shutdown(sock, SD_BOTH); }
    inline int lastSocketError() { return ::WSAGetLastError(); }
    inline constexpr int kConnResetError = WSAECONNRESET;
#else
    using RawSocket = int;
    inline constexpr RawSocket kInvalidSocket = -1;
    inline void closeRawSocket(RawSocket sock) { ::close(sock); }
    inline void shutdownRawSocket(RawSocket sock) { ::shutdown(sock, SHUT_RDWR); }
    inline int lastSocketError() { return errno; }
    inline constexpr int kConnResetError = ECONNRESET;
#endif

    // Suppress SIGPIPE on a write to a closed peer where the platform offers it
    // as a send flag (Linux); elsewhere it is zero and the send behaves as an
    // ordinary one.
#ifdef MSG_NOSIGNAL
    inline constexpr int kSendFlags = MSG_NOSIGNAL;
#else
    inline constexpr int kSendFlags = 0;
#endif
} // namespace bqui::agent
