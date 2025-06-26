#pragma once

#include "usage.h"

#include "asevisibility.h"

#include <memory>
#include <stdint.h>

namespace ase
{
    class IndexBufferImpl;
    class RenderContext;
    class Platform;
    class Buffer;
    struct Async;

    class ASE_EXPORT IndexBuffer
    {
    public:
        IndexBuffer(std::shared_ptr<IndexBufferImpl> impl);
        IndexBuffer(IndexBuffer const& other) = default;
        IndexBuffer(IndexBuffer&& other) = default;
        ~IndexBuffer();

        IndexBuffer& operator=(IndexBuffer const& other) = default;
        IndexBuffer& operator=(IndexBuffer&& other) = default;

        bool operator==(IndexBuffer const& rhs) const;
        bool operator!=(IndexBuffer const& rhs) const;
        bool operator<(IndexBuffer const& rhs) const;

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
        inline IndexBufferImpl* d() { return deferred_.get(); }
        inline IndexBufferImpl const* d() const { return deferred_.get(); }
        std::shared_ptr<IndexBufferImpl> deferred_;
    };
}

