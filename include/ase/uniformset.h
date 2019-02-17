#pragma once

#include <btl/visibility.h>

#include <memory>

namespace ase
{
    class UniformBufferRange;
    class UniformSetImpl;

    class BTL_VISIBLE UniformSet
    {
    public:
        explicit UniformSet(std::shared_ptr<UniformSetImpl> impl);

        bool operator==(UniformSet const& rhs) const;
        bool operator!=(UniformSet const& rhs) const;
        bool operator<(UniformSet const& rhs) const;
        bool operator>(UniformSet const& rhs) const;

        void bindUniformBufferRange(
                int binding,
                UniformBufferRange const& buffer
                );

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
        inline UniformSetImpl* d() { return deferred_.get(); }
        inline UniformSetImpl const* d() const { return deferred_.get(); }

    private:
        std::shared_ptr<UniformSetImpl> deferred_;
    };
} // namespace ase

