#ifdef _WIN32

#include "pipetransport.h"

#include <memory>
#include <string>
#include <utility>

namespace bqui::agent
{

namespace
{

class PipeListener : public TransportListener
{
public:
    explicit PipeListener(std::string endpoint) : endpoint_(std::move(endpoint))
    {
    }

    std::unique_ptr<Transport> accept() override
    {
        // FILE_FLAG_OVERLAPPED: the transport reads and writes concurrently
        // from two threads, which a synchronous handle would serialise.
        HANDLE handle = CreateNamedPipeA(
                endpoint_.c_str(),
                PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                PIPE_UNLIMITED_INSTANCES,
                65536, 65536, 0, nullptr);

        if (handle == INVALID_HANDLE_VALUE)
            throw lastError("CreateNamedPipe");

        // An overlapped ConnectNamedPipe completes asynchronously: wait on its
        // event unless the client already connected (ERROR_PIPE_CONNECTED).
        OVERLAPPED overlapped{};
        overlapped.hEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
        if (!overlapped.hEvent)
        {
            CloseHandle(handle);
            throw lastError("CreateEvent");
        }

        BOOL connected = ConnectNamedPipe(handle, &overlapped);
        DWORD error = connected ? ERROR_SUCCESS : GetLastError();

        if (!connected && error == ERROR_IO_PENDING)
        {
            DWORD ignored = 0;
            if (!GetOverlappedResult(handle, &overlapped, &ignored, TRUE))
            {
                CloseHandle(overlapped.hEvent);
                CloseHandle(handle);
                throw lastError("ConnectNamedPipe");
            }
        }
        else if (!connected && error != ERROR_PIPE_CONNECTED)
        {
            CloseHandle(overlapped.hEvent);
            CloseHandle(handle);
            throw lastError("ConnectNamedPipe");
        }

        CloseHandle(overlapped.hEvent);
        return std::make_unique<PipeTransport>(handle);
    }

private:
    std::string endpoint_;
};

} // namespace

std::unique_ptr<Transport> pipeConnect(std::string const& endpoint)
{
    // Race with the server: the pipe may not exist yet, or all its instances
    // may be busy between accepts. Retry briefly rather than failing.
    for (;;)
    {
        HANDLE handle = CreateFileA(
                endpoint.c_str(),
                GENERIC_READ | GENERIC_WRITE,
                0, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);

        if (handle != INVALID_HANDLE_VALUE)
            return std::make_unique<PipeTransport>(handle);

        DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND)
            Sleep(10);
        else if (error == ERROR_PIPE_BUSY)
            WaitNamedPipeA(endpoint.c_str(), 5000);
        else
            throw lastError("CreateFile");
    }
}

std::unique_ptr<TransportListener> pipeListen(std::string const& endpoint)
{
    return std::make_unique<PipeListener>(endpoint);
}

} // namespace bqui::agent

#endif
