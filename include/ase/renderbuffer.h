#pragma once

#include "format.h"
#include "vector.h"

#include "asevisibility.h"

#include <memory>

namespace ase
{
    class RenderbufferImpl;

    class ASE_EXPORT Renderbuffer
    {
    public:
        explicit Renderbuffer(std::shared_ptr<RenderbufferImpl> impl);

        Renderbuffer(Renderbuffer const& rhs) = default;
        Renderbuffer(Renderbuffer&& rhs) noexcept = default;

        /*bool operator==(Renderbuffer const& other) const;
        bool operator!=(Renderbuffer const& other) const;
        bool operator<(Renderbuffer const& other) const;*/

        Format getFormat() const;
        Vector2i getSize() const;

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
        inline RenderbufferImpl* d() { return deferred_.get(); }
        inline RenderbufferImpl const* d() const { return deferred_.get(); }

    private:
        std::shared_ptr<RenderbufferImpl> deferred_;
    };
} // namespace ase

