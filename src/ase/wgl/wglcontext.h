#pragma once

#include <wingdi.h>

#include <mutex>

namespace ase
{
    class WglWindow;
    class WglPlatform;

    class WglContext
    {
    public:
        typedef std::mutex Mutex;
        typedef std::unique_lock<Mutex> Lock;

        WglContext();
        WglContext(WglPlatform& platform);
        WglContext(WglContext&& other);
        WglContext(WglContext const&) = delete;
        ~WglContext();

        WglContext& operator=(WglContext const&) = delete;
        WglContext& operator=(WglContext&& other);

        void makeCurrent(Lock const& lock, HDC hdc) const;

    private:
        WglPlatform* platform_;

    };
} // namespace ase

