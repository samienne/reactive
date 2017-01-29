#pragma once

#include "usage.h"
#include "namedvertexspec.h"

#include <memory>

namespace ase
{
    class RenderContext;
    class VertexBufferImpl;
    class Platform;
    class Buffer;
    class Aabb;
    struct Async;

    /**
     * @brief Hardware buffer for vertex data.
     */
    class VertexBuffer
    {
    public:
        VertexBuffer();
        VertexBuffer(VertexBuffer const& other) = default;
        VertexBuffer(VertexBuffer&& other) = default;
        VertexBuffer(RenderContext& context, Buffer const& buffer, Usage usage);
        VertexBuffer(RenderContext& context, Buffer const& buffer, Usage usage,
                Async async);
        ~VertexBuffer();

        VertexBuffer& operator=(VertexBuffer const& other) = default;
        VertexBuffer& operator=(VertexBuffer&& other) = default;

        bool operator==(VertexBuffer const& other) const;
        bool operator!=(VertexBuffer const& other) const;
        bool operator<(VertexBuffer const& other) const;

        operator bool() const;

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
        inline VertexBufferImpl* d() { return deferred_.get(); }
        inline VertexBufferImpl const* d() const { return deferred_.get(); }

    private:
        std::shared_ptr<VertexBufferImpl> deferred_;
    };
}

