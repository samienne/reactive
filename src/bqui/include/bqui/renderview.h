#pragma once

#include "bquivisibility.h"

#include <ase/framebuffer.h>
#include <ase/vector.h>

#include <chrono>
#include <functional>
#include <memory>
#include <vector>

namespace ase
{
    class RenderContext;
}

namespace bqui
{
    class RenderHandle;
    class RenderFrame;

    namespace detail
    {
        struct RenderHandleCore;
        struct RenderViewState;
    } // namespace detail

    /** @brief A view a RenderHandle renders into, shown by
     * widget::renderView().
     *
     * Copies are the same view. A view holds one colour texture sized to the
     * pixel size it is drawn at; mount it in at most one place at a time.
     */
    class BQUI_EXPORT RenderView
    {
    public:
        explicit RenderView(std::shared_ptr<detail::RenderViewState> state);

        RenderView(RenderView const&) = default;
        RenderView(RenderView&&) noexcept = default;
        RenderView& operator=(RenderView const&) = default;
        RenderView& operator=(RenderView&&) noexcept = default;

        bool operator==(RenderView const& rhs) const;
        bool operator!=(RenderView const& rhs) const;

        std::shared_ptr<detail::RenderViewState> const& getState() const;

    private:
        std::shared_ptr<detail::RenderViewState> state_;
    };

    /** @brief One view to render this frame. */
    struct RenderTarget
    {
        RenderView view;

        /** @brief Colour (FORMAT_SRGBA, premultiplied) plus FORMAT_DEPTH16
         * depth, both @ref size pixels. Owned by the view.
         *
         * Rendered with GL clip space: clip-space y up is the view's top.
         */
        ase::Framebuffer framebuffer;

        ase::Vector2i size;

        /** @brief Whether the targets are new since the last frame; their
         * content is then undefined. */
        bool resized;
    };

    /** @brief What a RenderHandle's callback renders in one window frame. */
    class BQUI_EXPORT RenderFrame
    {
    public:
        RenderFrame(ase::RenderContext& context,
                std::chrono::microseconds time,
                std::chrono::microseconds dt,
                std::vector<RenderTarget> targets);

        /** @brief The context drawing the window; submit to its main queue. */
        ase::RenderContext& getRenderContext() const;

        /** @brief The window's frame clock. */
        std::chrono::microseconds getTime() const;
        std::chrono::microseconds getDt() const;

        /** @brief The handle's views drawn in this window, at least one. */
        std::vector<RenderTarget> const& getTargets() const;

        /** @brief Asks for one more frame after this one. */
        void requestFrame();

        bool isFrameRequested() const;

    private:
        ase::RenderContext* context_;
        std::chrono::microseconds time_;
        std::chrono::microseconds dt_;
        std::vector<RenderTarget> targets_;
        bool frameRequested_ = false;
    };

    /** @brief Owns a per-frame render callback and the views it renders.
     *
     * The callback runs on the app thread inside a window's frame, after the
     * widgets are drawn and before the window is painted, once per window
     * that shows any of the handle's views. It runs when a view is new or
     * resized, when a frame was requested, and every frame while continuous.
     * Commands it submits to the frame's main queue land before the window's
     * paint, so the views show them in the same frame.
     *
     * Destroying the handle stops the callback; its views then keep their
     * last content.
     */
    class BQUI_EXPORT RenderHandle
    {
    public:
        using Callback = std::function<void(RenderFrame&)>;

        explicit RenderHandle(Callback callback);

        RenderHandle(RenderHandle const&) = delete;
        RenderHandle(RenderHandle&&) noexcept = default;
        RenderHandle& operator=(RenderHandle const&) = delete;
        RenderHandle& operator=(RenderHandle&& rhs) noexcept;

        ~RenderHandle();

        RenderView makeView();

        /** @brief Asks for one more callback. Thread-safe. */
        void requestFrame();

        /** @brief Runs the callback every frame while @p continuous.
         * Thread-safe. */
        void setContinuous(bool continuous);

    private:
        void detach();

        std::shared_ptr<detail::RenderHandleCore> core_;
    };
} // namespace bqui
