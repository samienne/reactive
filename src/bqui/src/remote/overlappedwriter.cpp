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

// A failure to create the event surfaces on the first write, so the owning
// transport's constructor cannot throw past the handles it already holds.
OverlappedWriter::OverlappedWriter(HANDLE handle) :
    handle_(handle),
    event_(CreateEventA(nullptr, TRUE, FALSE, nullptr))
{
    if (!event_)
        eventError_ = GetLastError();
}

OverlappedWriter::~OverlappedWriter()
{
    cancel();
    if (event_)
        CloseHandle(event_);
}

size_t OverlappedWriter::write(void const* data, size_t size)
{
    if (!event_)
        throw writeError("CreateEvent", eventError_);

    if (size == 0 || pending())
        return 0;

    size_t taken = size < sizeof(chunk_) ? size : sizeof(chunk_);
    std::memcpy(chunk_, data, taken);
    chunkOffset_ = 0;
    chunkSize_ = taken;

    start();
    return taken;
}

bool OverlappedWriter::pending()
{
    if (!pending_)
        return false;

    DWORD wrote = 0;
    if (!GetOverlappedResult(handle_, &overlapped_, &wrote, FALSE))
    {
        DWORD error = GetLastError();
        if (error == ERROR_IO_INCOMPLETE)
            return true;

        pending_ = false;
        throw writeError("GetOverlappedResult", error);
    }

    pending_ = false;
    if (wrote == 0)
        throw std::runtime_error("WriteFile wrote nothing");

    chunkOffset_ += wrote;
    if (chunkOffset_ == chunkSize_)
        return false;

    start();
    return true;
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

void OverlappedWriter::start()
{
    ResetEvent(event_);
    overlapped_ = OVERLAPPED{};
    overlapped_.hEvent = event_;

    if (!WriteFile(handle_, chunk_ + chunkOffset_,
                static_cast<DWORD>(chunkSize_ - chunkOffset_), nullptr,
                &overlapped_))
    {
        DWORD error = GetLastError();
        if (error != ERROR_IO_PENDING)
            throw writeError("WriteFile", error);
    }

    pending_ = true;
}

} // namespace bqui::remote

#endif
