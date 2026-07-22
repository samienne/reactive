#ifdef _WIN32

#include "pipetransport.h"

#include <string>

namespace bqui::agent
{

std::runtime_error lastError(char const* what)
{
    return std::runtime_error(std::string(what) + " failed: "
            + std::to_string(GetLastError()));
}

namespace
{
    // A manual-reset, initially-nonsignaled event, or a throw on failure.
    HANDLE makeEvent()
    {
        HANDLE event = CreateEventA(nullptr, TRUE, FALSE, nullptr);
        if (!event)
            throw lastError("CreateEvent");
        return event;
    }
} // namespace

PipeTransport::PipeTransport(HANDLE handle)
    : handle_(handle),
      readEvent_(makeEvent()),
      writeEvent_(makeEvent()),
      stopEvent_(makeEvent())
{
}

PipeTransport::~PipeTransport()
{
    if (handle_ != INVALID_HANDLE_VALUE)
        CloseHandle(handle_);

    CloseHandle(readEvent_);
    CloseHandle(writeEvent_);
    CloseHandle(stopEvent_);
}

void PipeTransport::writeAll(void const* data, size_t size)
{
    auto bytes = static_cast<char const*>(data);
    size_t written = 0;
    while (written < size)
    {
        OVERLAPPED overlapped{};
        overlapped.hEvent = writeEvent_;
        ResetEvent(writeEvent_);

        DWORD wrote = 0;
        BOOL ok = WriteFile(handle_, bytes + written,
                static_cast<DWORD>(size - written), &wrote, &overlapped);
        if (!ok)
        {
            if (GetLastError() != ERROR_IO_PENDING)
                throw lastError("WriteFile");

            // The write is in flight; block for it to finish (a send is never
            // cancelled — only the parked read is, on shutdown).
            if (!GetOverlappedResult(handle_, &overlapped, &wrote, TRUE))
                throw lastError("GetOverlappedResult");
        }

        written += wrote;
    }
}

bool PipeTransport::readAll(void* data, size_t size)
{
    auto bytes = static_cast<char*>(data);
    size_t read = 0;
    while (read < size)
    {
        // A close() may have landed between chunks; stop before blocking again.
        if (closing_.load(std::memory_order_acquire))
            return false;

        OVERLAPPED overlapped{};
        overlapped.hEvent = readEvent_;
        ResetEvent(readEvent_);

        DWORD got = 0;
        BOOL ok = ReadFile(handle_, bytes + read,
                static_cast<DWORD>(size - read), &got, &overlapped);
        if (!ok)
        {
            DWORD error = GetLastError();
            if (error == ERROR_BROKEN_PIPE)
                return false; // Peer closed before this read even started.

            if (error != ERROR_IO_PENDING)
                throw lastError("ReadFile");

            // The read is in flight. Wait for it to complete, or for close() to
            // signal the stop event. The stop event is manual-reset, so a read
            // issued after close() finds it already set — there is no window in
            // which a read can park unseen.
            HANDLE waits[2] = { readEvent_, stopEvent_ };
            DWORD which = WaitForMultipleObjects(2, waits, FALSE, INFINITE);
            if (which == WAIT_OBJECT_0 + 1)
            {
                // Stop requested: cancel the pending read and reap it, so the
                // kernel is done with this stack OVERLAPPED before we return.
                CancelIoEx(handle_, &overlapped);
                GetOverlappedResult(handle_, &overlapped, &got, TRUE);
                return false;
            }

            if (!GetOverlappedResult(handle_, &overlapped, &got, TRUE))
            {
                DWORD resultError = GetLastError();
                if (resultError == ERROR_BROKEN_PIPE
                        || resultError == ERROR_OPERATION_ABORTED)
                    return false;

                throw lastError("GetOverlappedResult");
            }
        }

        if (got == 0)
            return false;

        read += got;
    }

    return true;
}

void PipeTransport::close()
{
    // Set the flag first (a read not yet parked bails on it), then the stop
    // event (a read already parked, or about to park, wakes on it). Manual-reset
    // keeps the event set for every later read. CancelIoEx from another thread
    // is not needed — the parked read cancels its own OVERLAPPED after the wait.
    closing_.store(true, std::memory_order_release);
    SetEvent(stopEvent_);
}

} // namespace bqui::agent

#endif
