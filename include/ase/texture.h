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
        explicit Texture(std::shared_ptr<TextureImpl> impl);
        Texture(Texture const& other) = default;
        Texture(Texture&& other) = default;
        ~Texture();

        Texture& operator=(Texture const& rhs) = default;
        Texture& operator=(Texture&& rhs) = default;

        bool operator==(Texture const& rhs) const;
        bool operator!=(Texture const& rhs) const;
        bool operator<=(Texture const& rhs) const;
        bool operator<(Texture const& rhs) const;

        Format getFormat();
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

