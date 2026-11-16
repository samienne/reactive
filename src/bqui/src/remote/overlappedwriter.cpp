#ifdef _WIN32

#include "overlappedwriter.h"

#include <cstring>
#include <stdexcept>
#include <string>

namespace bqui::remote
{

namespace
{
    std::runtime_error writeError(char const* what, DWORD error)
    {
        return std::runtime_error(std::string(what) + " failed: "
                + std::to_string(error));
    }
} // namespace

OverlappedWriter::OverlappedWriter(HANDLE handle) :
    handle_(handle),
    event_(CreateEventA(nullptr, TRUE, FALSE, nullptr))
{
    if (!event_)
        throw writeError("CreateEvent", GetLastError());
}

OverlappedWriter::~OverlappedWriter()
{
    cancel();
    CloseHandle(event_);
}

size_t OverlappedWriter::write(void const* data, size_t size)
{
    if (size == 0 || pending())
        return 0;

    size_t taken = size < sizeof(chunk_) ? size : sizeof(chunk_);
    std::memcpy(chunk_, data, taken);

    ResetEvent(event_);
    overlapped_ = OVERLAPPED{};
    overlapped_.hEvent = event_;

    DWORD wrote = 0;
    if (WriteFile(handle_, chunk_, static_cast<DWORD>(taken), &wrote,
                &overlapped_))
        return wrote;

    DWORD error = GetLastError();
    if (error != ERROR_IO_PENDING)
        throw writeError("WriteFile", error);

    // An overlapped write completes whole or fails, so the chunk is taken.
    pending_ = true;
    return taken;
}

bool OverlappedWriter::pending()
{
    if (!pending_)
        return false;

    DWORD wrote = 0;
    if (GetOverlappedResult(handle_, &overlapped_, &wrote, FALSE))
    {
        pending_ = false;
        return false;
    }

    DWORD error = GetLastError();
    if (error == ERROR_IO_INCOMPLETE)
        return true;

    pending_ = false;
    throw writeError("GetOverlappedResult", error);
}

void OverlappedWriter::cancel()
{
    if (!pending_)
        return;

    CancelIoEx(handle_, &overlapped_);
    DWORD wrote = 0;
    GetOverlappedResult(handle_, &overlapped_, &wrote, TRUE);
    pending_ = false;
}

} // namespace bqui::remote

#endif
