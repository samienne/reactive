#pragma once

#include "reactive/reactivevisibility.h"

#include <chrono>
#include <stdint.h>

namespace reactive::signal
{
    class REACTIVE_EXPORT FrameInfo
    {
    public:
        inline FrameInfo(uint64_t frameId, std::chrono::microseconds dt) :
            frameId_(frameId),
            dt_(dt)
        {
        }

        inline uint64_t getFrameId() const
        {
            return frameId_;
        }

        inline std::chrono::microseconds getDeltaTime() const
        {
            return dt_;
        }

    private:
        uint64_t frameId_;
        std::chrono::microseconds dt_;
    };
} // reactive::signal

