#pragma once

#include "vector.h"
#include "format.h"

#include <btl/visibility.h>

#include <memory>

namespace ase
{
    class TextureImpl;
    class RenderContext;
    class Platform;
    class Buffer;
    struct Async;

    class BTL_VISIBLE Texture
    {
    public:
        Texture();
        Texture(Texture const& other) = default;
        Texture(Texture&& other) = default;
        Texture(RenderContext& context, Vector2i const& size, Format format,
                Buffer const& buffer);
        Texture(RenderContext& context, Vector2i const& size, Format format,
                Buffer const& buffer, Async async);
        ~Texture();

        Texture& operator=(Texture const& other) = default;
        Texture& operator=(Texture&& other) = default;

        bool operator==(Texture const& other) const;
        bool operator!=(Texture const& other) const;
        bool operator<=(Texture const& other) const;

        Format getFormat();
        operator bool() const;
        Vector2i getSize() const;

        template <class T>
        T const& getImpl() const
        {
            return reinterpret_cast<T const&>(*d());
        }

    private:
        std::shared_ptr<TextureImpl> deferred_;
        inline TextureImpl* d() { return deferred_.get(); }
        inline TextureImpl const* d() const { return deferred_.get(); }

        Format format_ = FORMAT_UNKNOWN;
    };
}

