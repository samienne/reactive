#pragma once

#include "presentstatus.h"

namespace ase
{
    class CommandBuffer;
    class Window;

    class RenderQueueImpl
    {
    public:
        virtual ~RenderQueueImpl() = default;
        virtual void flush() = 0;
        virtual void finish() = 0;
        virtual void submit(CommandBuffer&& renderQueue) = 0;

        /** @brief Sequence a present of `window` relative to this queue's
         * submitted draws. */
        virtual PresentStatus present(Window& window) = 0;
    };
} // namespace ase

