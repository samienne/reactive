#pragma once

#include <cstddef>
#include <cstring>
#include <type_traits>

namespace btl
{
    /** @brief An opaque, non-owning reference to an OS handle.
     *
     * Carries a platform handle (a POSIX fd, a Win32 SOCKET or HANDLE, ...)
     * without exposing its type, so a NativeHandle can pass through headers that
     * must not pull in winsock or windows.h. It is a trivially copyable value
     * with no ownership: it names a handle the caller still owns and must keep
     * alive while registered. Build one only through the per-platform conversion
     * headers (btl/posix/nativehandle_posix.h, btl/win32/nativehandle_win32.h).
     */
    class NativeHandle
    {
    public:
        /** @brief What the stored bytes are, so a backend waits the right way. */
        enum class Kind
        {
            None,
            Fd,     // POSIX file descriptor: select/poll.
            Socket, // Win32 SOCKET: WSAEventSelect onto an event.
            Handle, // Win32 waitable HANDLE (e.g. an overlapped-I/O event).
        };

        NativeHandle() = default;

        bool valid() const noexcept
        {
            return kind_ != Kind::None;
        }

        Kind kind() const noexcept
        {
            return kind_;
        }

    private:
        friend struct NativeHandleAccess;

        alignas(alignof(std::max_align_t)) unsigned char
            storage_[2 * sizeof(void*)] = {};
        Kind kind_ = Kind::None;
    };

    /** @brief The one door platform conversion code uses to fill or read a
     * NativeHandle's bytes. Not for general use.
     */
    struct NativeHandleAccess
    {
        template <typename T>
        static NativeHandle make(T value, NativeHandle::Kind kind)
        {
            static_assert(std::is_trivially_copyable<T>::value,
                    "native handle payload must be trivially copyable");
            static_assert(sizeof(T) <= sizeof(NativeHandle::storage_),
                    "native handle payload does not fit; bump the storage size");

            NativeHandle handle;
            std::memcpy(handle.storage_, &value, sizeof(T));
            handle.kind_ = kind;
            return handle;
        }

        template <typename T>
        static T load(NativeHandle const& handle)
        {
            T value{};
            std::memcpy(&value, handle.storage_, sizeof(T));
            return value;
        }
    };
}
