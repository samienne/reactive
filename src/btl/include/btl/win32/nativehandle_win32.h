#pragma once

#include <btl/nativehandle.h>

// Pulls in winsock/windows, so this header stays confined to platform .cpp
// files.
#include <winsock2.h>
#include <windows.h>

namespace btl
{
    /** @brief Wrap a Win32 SOCKET as a NativeHandle. */
    inline NativeHandle fromSocket(SOCKET socket)
    {
        return makeNativeHandle(socket, NativeHandle::Kind::Socket);
    }

    /** @brief Read the SOCKET back out. Only valid on Win32 socket handles. */
    inline SOCKET toSocket(NativeHandle const& handle)
    {
        return loadNativeHandle<SOCKET>(handle);
    }

    /** @brief Wrap a Win32 waitable HANDLE (e.g. an overlapped-I/O event) as a
     * NativeHandle. The loop waits on it directly and never closes it.
     */
    inline NativeHandle fromHandle(HANDLE handle)
    {
        return makeNativeHandle(handle, NativeHandle::Kind::Handle);
    }

    /** @brief Read the HANDLE back out. Only valid on Win32 handle handles. */
    inline HANDLE toHandle(NativeHandle const& native)
    {
        return loadNativeHandle<HANDLE>(native);
    }
}
