#pragma once

#include "rendercommand.h"
#include "asevisibility.h"

#include <btl/future.h>

#include <algorithm>
#include <vector>
#include <optional>
#include <variant>

namespace ase
{
    class ASE_EXPORT CommandBuffer
    {
    public:
        using Iterator = std::vector<RenderCommand>::iterator;
        using ConstIterator = std::vector<RenderCommand>::const_iterator;

        /**
         * @brief Push a draw, limited to `scissor` when one is given.
         */
        void push(
                Framebuffer framebuffer,
                Pipeline pipeline,
                UniformSet uniforms,
                VertexBuffer vertexBuffer,
                std::optional<IndexBuffer> indexBuffer,
                std::vector<Texture> textures,
                float z,
                std::optional<ScissorRect> scissor = std::nullopt);

        /**
         * @brief Clear the chosen buffers of `target`, limited to `scissor`
         * when one is given.
         */
        void pushClear(Framebuffer target,
                float r = 0.0f,
                float g = 0.0f,
                float b = 0.0f,
                float a = 1.0f,
                bool color = true,
                bool depth = true,
                bool stencil = false,
                std::optional<ScissorRect> scissor = std::nullopt
                );

        void pushFence(std::function<void()> completeCb);
        btl::future::Future<> pushFence();

        /**
         * @brief Keep the draws pushed so far ahead of those pushed next.
         *
         * Draws are otherwise free to be reordered up to the next
         * non-draw command.
         */
        void pushBarrier();

        void pushUpload(
                std::variant<VertexBuffer, IndexBuffer, UniformBuffer> target,
                Buffer data,
                Usage usage
                );

        /**
         * @brief Upload `data` as the whole of `target`.
         *
         * `data` holds `size` tightly packed pixels in `format`, which
         * must be the size and format `target` was made with: a texture
         * never changes either after `makeTexture`.
         */
        void pushUpload(Texture target, Buffer data, Vector2i size,
                Format format);

        /**
         * @brief Upload `data` into the `size` region of `target` at `offset`.
         *
         * `data` holds `size` tightly packed pixels in the texture's format.
         * The region must lie inside the texture; its size and format do not
         * change, so a texture may be filled piecewise after `makeTexture`.
         */
        void pushSubUpload(Texture target, Buffer data, Vector2i offset,
                Vector2i size);

        size_t size() const;
        Iterator begin();
        Iterator end();

        ConstIterator begin() const;
        ConstIterator end() const;

    private:
        std::vector<RenderCommand> commands_;
    };

    /**
     * @brief Stable-sorts each run of consecutive depth-tested draws.
     *
     * Every other command in [first, last) keeps its place and ends the run
     * before it: a draw whose pipeline does not test depth as well as a
     * clear, an upload, a fence or a barrier.
     */
    template <class Iterator, class Compare>
    void sortDrawRuns(Iterator first, Iterator last, Compare compare)
    {
        auto isSortable = [](RenderCommand const& command)
        {
            auto const* draw = std::get_if<DrawCommand>(&command);
            return draw && draw->getPipeline().isDepthTestEnabled();
        };

        std::vector<Iterator> order;
        std::vector<RenderCommand> sorted;

        while (first != last)
        {
            first = std::find_if(first, last, isSortable);
            auto runEnd = std::find_if_not(first, last, isSortable);

            order.clear();
            for (auto i = first; i != runEnd; ++i)
                order.push_back(i);

            std::stable_sort(order.begin(), order.end(),
                    [&compare](Iterator a, Iterator b)
                    {
                        return compare(*a, *b);
                    });

            sorted.clear();
            for (auto i : order)
                sorted.push_back(std::move(*i));
            std::move(sorted.begin(), sorted.end(), first);

            first = runEnd;
        }
    }
} // namespace ase


