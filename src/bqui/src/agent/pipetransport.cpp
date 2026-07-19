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

PipeTransport::PipeTransport(HANDLE handle) : handle_(handle) {}

PipeTransport::~PipeTransport()
{
    if (handle_ != INVALID_HANDLE_VALUE)
        CloseHandle(handle_);
}

void PipeTransport::writeAll(void const* data, size_t size)
{
    auto bytes = static_cast<char const*>(data);
    size_t written = 0;
    while (written < size)
    {
        DWORD wrote = 0;
        if (!WriteFile(handle_, bytes + written,
                    static_cast<DWORD>(size - written), &wrote, nullptr))
            throw lastError("WriteFile");

        written += wrote;
    }
}

bool PipeTransport::readAll(void* data, size_t size)
{
    auto bytes = static_cast<char*>(data);
    size_t read = 0;
    while (read < size)
    {
        DWORD got = 0;
        if (!ReadFile(handle_, bytes + read,
                    static_cast<DWORD>(size - read), &got, nullptr))
        {
            DWORD error = GetLastError();
            if (error == ERROR_BROKEN_PIPE)
                return false;

            throw lastError("ReadFile");
        }

        if (got == 0)
            return false;

        read += got;
    }

    return true;
}

} // namespace bqui::agent

#endif
