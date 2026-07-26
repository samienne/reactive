#pragma once

#include <btl/nativehandle.h>

// Pulls in winsock, so this header stays confined to platform .cpp files.
#include <winsock2.h>

namespace btl
{
    /** @brief Wrap a Win32 SOCKET as a NativeHandle. */
    inline NativeHandle fromSocket(SOCKET socket)
    {
        return NativeHandleAccess::make(socket);
    }

    /** @brief Read the SOCKET back out. Only valid on Win32 socket handles. */
    inline SOCKET toSocket(NativeHandle const& handle)
    {
        return NativeHandleAccess::load<SOCKET>(handle);
    }
}
