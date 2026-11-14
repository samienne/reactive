#pragma once

#include "uniformbuffer.h"
#include "uniformbufferrange.h"

#include "asevisibility.h"

#include <vector>

namespace ase
{
    class RenderContext;
    class CommandBuffer;

    /**
     * @brief Per-frame storage for uniform block contents, uploaded once a
     * frame.
     *
     * The ranges a frame binds stay valid while the GPU is still drawing
     * it; framesInFlight is how many frames may be in that state at once.
     * Per frame:
     *
     *     ring.beginFrame();
     *     auto range = ring.write(matrices);  // UniformBlock or UniformValues
     *     set.bindUniformBufferRange("MatrixBlock", range);
     *     ...
     *     ring.endFrame(uploads);
     *     queue.submit(std::move(uploads));     // before the frame's draws
     *
     * endFrame pushes the frame's one upload, which must execute before the
     * draws that bind the frame's ranges. Writing outside beginFrame and
     * endFrame throws std::logic_error. Each range starts at a multiple of
     * the context's uniform buffer offset alignment.
     */
    class ASE_EXPORT UniformRing
    {
    public:
        explicit UniformRing(RenderContext& context, int framesInFlight = 3);
        UniformRing(UniformRing const&) = delete;
        UniformRing(UniformRing&&) noexcept = default;
        ~UniformRing();

        UniformRing& operator=(UniformRing const&) = delete;
        UniformRing& operator=(UniformRing&&) noexcept = default;

        void beginFrame();

        /**
         * @brief Copies size bytes into the frame's buffer and returns where
         * they will be.
         */
        UniformBufferRange write(void const* data, size_t size);

        template <class Block>
        UniformBufferRange write(Block const& block)
        {
            return write(block.data(), block.getSize());
        }

        void endFrame(CommandBuffer& uploads);

        size_t getAlignment() const;
        int getFramesInFlight() const;

    private:
        struct Frame
        {
            UniformBuffer buffer;
            std::vector<char> staging;
        };

        size_t alignment_;
        std::vector<Frame> frames_;
        size_t current_;
        size_t written_ = 0;
        bool open_ = false;
    };
} // namespace ase
