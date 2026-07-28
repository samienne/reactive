#pragma once

#include <btl/nativehandle.h>

namespace btl
{
    /** @brief Wrap a POSIX file descriptor as a NativeHandle. */
    inline NativeHandle fromFd(int fd)
    {
        return makeNativeHandle(fd, NativeHandle::Kind::Fd);
    }

    /** @brief Read the file descriptor back out. Only valid on POSIX handles. */
    inline int toFd(NativeHandle const& handle)
    {
        return loadNativeHandle<int>(handle);
    }
}
