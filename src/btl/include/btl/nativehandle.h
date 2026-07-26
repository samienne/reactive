#pragma once

#include <cstddef>
#include <cstring>
#include <type_traits>

namespace btl
{
    /** @brief An opaque, non-owning reference to an OS handle.
     *
     * Carries a platform handle (a POSIX fd, a Win32 SOCKET, ...) without
     * exposing its type, so a NativeHandle can pass through headers that must
     * not pull in winsock or windows.h. It is a trivially copyable value with no
     * ownership: it names a handle the caller still owns and must keep alive
     * while registered. Build one only through the per-platform conversion
     * headers (btl/posix/nativehandle_posix.h, btl/win32/nativehandle_win32.h).
     */
    class NativeHandle
    {
    public:
        NativeHandle() = default;

        bool valid() const noexcept
        {
            return valid_;
        }

    private:
        friend struct NativeHandleAccess;

        alignas(alignof(std::max_align_t)) unsigned char
            storage_[2 * sizeof(void*)] = {};
        bool valid_ = false;
    };

    /** @brief The one door platform conversion code uses to fill or read a
     * NativeHandle's bytes. Not for general use.
     */
    struct NativeHandleAccess
    {
        template <typename T>
        static NativeHandle make(T value)
        {
            static_assert(std::is_trivially_copyable<T>::value,
                    "native handle payload must be trivially copyable");
            static_assert(sizeof(T) <= sizeof(NativeHandle::storage_),
                    "native handle payload does not fit; bump the storage size");

            NativeHandle handle;
            std::memcpy(handle.storage_, &value, sizeof(T));
            handle.valid_ = true;
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
