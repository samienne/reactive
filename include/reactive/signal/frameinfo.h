#pragma once

#include <btl/hidden.h>

#include <chrono>
#include <stdint.h>

BTL_VISIBILITY_PUSH_HIDDEN

namespace reactive
{
    namespace signal
    {
        class BTL_VISIBLE FrameInfo
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
    } // signal
} // reactive

BTL_VISIBILITY_POP

