#pragma once

#include <btl/runloop.h>

namespace btl
{
    // The platform-agnostic surface RunLoop forwards to. One concrete subclass
    // per platform (posix/win32/...), built by makeRunLoopImpl().
    class RunLoopImpl
    {
    public:
        virtual ~RunLoopImpl() = default;

        virtual SourceId addReadable(NativeHandle, std::function<void()>) = 0;
        virtual SourceId addWritable(NativeHandle, std::function<void()>) = 0;
        virtual TimerId addTimer(std::chrono::microseconds,
                std::function<void()>) = 0;
        virtual void post(std::function<void()>) = 0;
        virtual void remove(SourceId) = 0;
        virtual void cancel(TimerId) = 0;
        virtual void run() = 0;
        virtual void stop() = 0;
    };

    std::unique_ptr<RunLoopImpl> makeRunLoopImpl();
}
