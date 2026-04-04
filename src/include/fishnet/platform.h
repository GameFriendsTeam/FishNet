#pragma once

/*
 * FishNet Platform Abstraction
 *
 * Provides cross-platform socket types and operations.
 * On Windows: WinSock2 with NOMINMAX/WIN32_LEAN_AND_MEAN.
 * On Linux/macOS: POSIX sockets (sys/socket, netinet/in, arpa/inet).
 *
 * Defines FISHNET_API export macro for shared library symbols.
 * Provides SocketHandle, initSockets(), closeSocket(), setNonBlocking(), etc.
 */

#include <cstdint>
#include <cstddef>

// Platform detection
#if defined(_WIN32) || defined(_WIN64)
    #define FISHNET_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define FISHNET_PLATFORM_LINUX 1
#elif defined(__APPLE__)
    #define FISHNET_PLATFORM_MACOS 1
#else
    #define FISHNET_PLATFORM_UNKNOWN 1
#endif

// Socket includes
#ifdef FISHNET_PLATFORM_WINDOWS
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <WinSock2.h>
    #include <WS2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <sys/types.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
#endif

// API export macros
#if defined(FISHNET_PLATFORM_WINDOWS) && !defined(FISHNET_STATIC)
    #ifdef FISHNET_EXPORTS
        #define FISHNET_API __declspec(dllexport)
    #else
        #define FISHNET_API __declspec(dllimport)
    #endif
#elif !defined(FISHNET_STATIC) && defined(__GNUC__)
    #define FISHNET_API __attribute__((visibility("default")))
#else
    #define FISHNET_API
#endif

// Cross-platform socket types
namespace fishnet {

#ifdef FISHNET_PLATFORM_WINDOWS
    using SocketHandle = SOCKET;
    constexpr SocketHandle InvalidSocket = INVALID_SOCKET;
    constexpr int SocketError = SOCKET_ERROR;
#else
    using SocketHandle = int;
    constexpr SocketHandle InvalidSocket = -1;
    constexpr int SocketError = -1;
#endif

// Platform socket operations
namespace platform {

    inline bool initSockets() {
    #ifdef FISHNET_PLATFORM_WINDOWS
        WSADATA wsaData;
        return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
    #else
        return true; // No init needed on POSIX
    #endif
    }

    inline void cleanupSockets() {
    #ifdef FISHNET_PLATFORM_WINDOWS
        WSACleanup();
    #endif
    }

    inline void closeSocket(SocketHandle sock) {
    #ifdef FISHNET_PLATFORM_WINDOWS
        closesocket(sock);
    #else
        close(sock);
    #endif
    }

    inline int getLastError() {
    #ifdef FISHNET_PLATFORM_WINDOWS
        return WSAGetLastError();
    #else
        return errno;
    #endif
    }

    inline bool setNonBlocking(SocketHandle sock) {
    #ifdef FISHNET_PLATFORM_WINDOWS
        u_long mode = 1;
        return ioctlsocket(sock, FIONBIO, &mode) == 0;
    #else
        int flags = fcntl(sock, F_GETFL, 0);
        if (flags == -1) return false;
        return fcntl(sock, F_SETFL, flags | O_NONBLOCK) != -1;
    #endif
    }

    inline void setRecvBufferSize(SocketHandle sock, int size) {
        setsockopt(sock, SOL_SOCKET, SO_RCVBUF,
                   reinterpret_cast<const char*>(&size), sizeof(size));
    }

} // namespace platform
} // namespace fishnet
