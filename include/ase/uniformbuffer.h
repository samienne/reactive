#pragma once

#include "buffer.h"
#include "usage.h"

#include "asevisibility.h"

#include <memory>

namespace ase
{
    class UniformBufferImpl;

    class ASE_EXPORT UniformBuffer
    {
    public:
        explicit UniformBuffer(std::shared_ptr<UniformBufferImpl> impl);
        UniformBuffer(UniformBuffer const&) = default;
        UniformBuffer(UniformBuffer&&) = default;

        UniformBuffer& operator=(UniformBuffer const&) = default;
        UniformBuffer& operator=(UniformBuffer&&) = default;

        void setData(Buffer buffer, Usage usage);

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
        inline UniformBufferImpl* d() { return deferred_.get(); }
        inline UniformBufferImpl const* d() const { return deferred_.get(); }

    private:
        std::shared_ptr<UniformBufferImpl> deferred_;
    };
} // namespace ase

