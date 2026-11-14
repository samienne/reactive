#pragma once

#include "renderqueueimpl.h"
#include "commandbuffer.h"

#include "asevisibility.h"

#include <vector>

namespace ase
{
    class ASE_EXPORT DummyRenderQueue : public RenderQueueImpl
    {
    public:
        /** @brief Headless render queue: nothing draws and fences complete
         * inline on submit(). */
        DummyRenderQueue() = default;

        // From RenderQueueImpl
        void flush() override;
        void finish() override;
        void submit(CommandBuffer&& renderQueue) override;

        /**
         * @brief Keep every command buffer submitted from now on, in
         * order, for a test to read back.
         */
        void recordSubmissions(bool record);
        std::vector<CommandBuffer> const& getSubmissions() const;

    private:
        bool recording_ = false;
        std::vector<CommandBuffer> submissions_;
    };
} // namespace ase
