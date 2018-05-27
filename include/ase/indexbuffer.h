#pragma once

#include "usage.h"

#include <btl/visibility.h>

#include <memory>
#include <stdint.h>

namespace ase
{
    class IndexBufferImpl;
    class RenderContext;
    class Platform;
    class Buffer;
    struct Async;

    class BTL_VISIBLE IndexBuffer
    {
    public:
        IndexBuffer();
        IndexBuffer(IndexBuffer const& other) = default;
        IndexBuffer(IndexBuffer&& other) = default;
        IndexBuffer(RenderContext& context, Buffer const& buffer, Usage usage);
        IndexBuffer(RenderContext& context, Buffer const& buffer, Usage usage,
                Async async);
        ~IndexBuffer();

        IndexBuffer& operator=(IndexBuffer const& other) = default;
        IndexBuffer& operator=(IndexBuffer&& other) = default;

        bool isEmpty() const;

        bool operator<(IndexBuffer const& other) const;
        operator bool() const;

        template <class T>
        T const& getImpl() const
        {
            return reinterpret_cast<T const&>(*d());
        }


    private:
        inline IndexBufferImpl* d() { return deferred_.get(); }
        inline IndexBufferImpl const* d() const { return deferred_.get(); }
        std::shared_ptr<IndexBufferImpl> deferred_;
    };
}

