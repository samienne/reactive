#pragma once

namespace ase
{
    class CommandBuffer;

    class RenderQueueImpl
    {
    public:
        virtual ~RenderQueueImpl() = default;
        virtual void flush() = 0;
        virtual void finish() = 0;
        virtual void submit(CommandBuffer&& renderQueue) = 0;
    };
} // namespace ase

