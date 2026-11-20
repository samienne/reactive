#pragma once

#include "format.h"

#include "asevisibility.h"

#include <memory>
#include <optional>

namespace ase
{
    class Texture;
    class Renderbuffer;
    class FramebufferImpl;

    /**
     * @brief The colour and depth targets a command draws into.
     *
     * Colour targets are textures; the depth target, stencil included, is
     * a renderbuffer. A change applies to the commands executed after it,
     * including ones already submitted.
     */
    class ASE_EXPORT Framebuffer
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

        /**
         * @brief Attaches a texture as colour target `index`, below 8.
         */
        void setColorTarget(size_t index, Texture texture);
        void unsetColorTarget(size_t index);

        void setDepthTarget(Renderbuffer buffer);
        void unsetDepthTarget();

        /**
         * @brief The format of colour attachment 0, `FORMAT_UNKNOWN` when
         * nothing is attached there.
         *
         * A window's framebuffer reports the format its platform asked the
         * pixel format for, which nothing verifies against the driver's
         * choice.
         */
        Format getColorFormat() const;

        /**
         * @brief The depth attachment's format, unset when there is none.
         *
         * `FORMAT_UNKNOWN` is a depth attachment of a format the backend
         * cannot tell.
         */
        std::optional<Format> getDepthFormat() const;

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

        template <class T>
        T& getImpl()
        {
            return reinterpret_cast<T&>(*d());
        }

    protected:
        inline FramebufferImpl* d() { return deferred_.get(); }
        inline FramebufferImpl const* d() const { return deferred_.get(); }

    private:
        std::shared_ptr<FramebufferImpl> deferred_;
    };
} // namespace ase


