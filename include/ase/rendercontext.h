#pragma once

#include <functional>
#include <vector>
#include <memory>

namespace ase
{
    class RenderCommand;
    class RenderTarget;
    class Window;
    class RenderContextImpl;
    class Platform;

    /**
     * @brief RenderContext
     *
     * Loading resources on a RenderContext makes them immediately
     * available for that context, although the upload might not have finished.
     * Using a resource with an unfinished upload might cause an implicit
     * wait until the resource is uploaded.
     *
     * Calling flush() will wait until all previous data uploads have finished
     * on that RenderContext.
     *
     * Flushing
     *
     * 1. Submit -- Sends commands to the rendering context.
     * 2. Flush  -- Waits for the submitted commands to be sent to the hw.
     *              context. Also waits for data upload to finish.
     * 3. Finish -- Waits for previously flushed contents to be finished.
     *              Makes all the previous data affected by commands and
     *              uploads available for other RenderContexts on the
     *              same Platform.
     *
     */
    class RenderContext
    {
    public:
        RenderContext(Platform& platform,
                std::shared_ptr<RenderContextImpl> impl);
        RenderContext(Platform& platform);
        RenderContext(RenderContext const&) = delete;
        RenderContext(RenderContext&&) = default;
        ~RenderContext();

        RenderContext& operator=(RenderContext const&) = delete;
        RenderContext& operator=(RenderContext&&) = default;

        void flush();
        void finish();
        void present(Window& window);

        Platform& getPlatform() const;

        template <class T>
        T const& getImpl() const
        {
            return reinterpret_cast<T const&>(*d());
        }

        template <class T>
        T& getImpl()
        {
            return reinterpret_cast<T&>(*d());
        }

    private:
        friend class RenderTarget;
        void submit(RenderTarget& target,
                std::vector<RenderCommand>&& commands);

    private:
        std::shared_ptr<RenderContextImpl> deferred_;
        inline RenderContextImpl* d() { return deferred_.get(); }
        inline RenderContextImpl const* d() const { return deferred_.get(); }

        Platform* platform_;
    };
}

