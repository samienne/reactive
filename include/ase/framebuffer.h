#pragma once

#include <btl/visibility.h>

#include <memory>

namespace ase
{
    class Texture;
    class Renderbuffer;
    class FramebufferImpl;

    class BTL_VISIBLE Framebuffer
    {
    public:
        Framebuffer(std::shared_ptr<FramebufferImpl> impl);
        Framebuffer(Framebuffer const& rhs) = default;
        Framebuffer(Framebuffer&& rhs) noexcept = default;

        Framebuffer& operator=(Framebuffer const& rhs) = default;
        Framebuffer& operator=(Framebuffer&& rhs) = default;

        bool operator==(Framebuffer const& other) const;
        bool operator!=(Framebuffer const& other) const;
        bool operator<(Framebuffer const& other) const;

        void setColorTarget(size_t index, Texture texture);
        void setColorTarget(size_t index, Renderbuffer buffer);
        void unsetColorTarget(size_t index);

        void setDepthTarget(Renderbuffer buffer);
        void unsetDepthTarget();

        void setStencilTarget(Renderbuffer buffer);
        void unsetStencilTarget();

        void clear();

        /**
         * @brief Casts the internal implementation to requested type.
         *
         * This is for internal use only and expects that you know the
         * actual type of implementation.
         */
        template <class T>
        T const& getImpl() const
        {
            return reinterpret_cast<T const&>(*d());
        }

    protected:
        inline FramebufferImpl* d() { return deferred_.get(); }
        inline FramebufferImpl const* d() const { return deferred_.get(); }

    private:
        std::shared_ptr<FramebufferImpl> deferred_;
    };
} // namespace ase


