#pragma once

#include "bq/bqvisibility.h"

#include <chrono>
#include <stdint.h>

namespace bq::signal
{
    class BQ_EXPORT FrameInfo
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
} // bq::signal

