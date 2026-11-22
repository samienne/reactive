#pragma once

#include "asevisibility.h"

#include <btl/future/future.h>

namespace ase
{
    class ASE_EXPORT FenceImpl
    {
    public:
        virtual ~FenceImpl() = default;

        virtual bool isSignaled() const = 0;
        virtual btl::future::Future<> whenSignaled() = 0;
    };
} // namespace ase
