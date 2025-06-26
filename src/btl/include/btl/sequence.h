#pragma once

#include "all.h"
#include "fmap.h"

namespace btl
{
    template <typename T>
    using IsSequence = btl::All<
        IsFunctor<T>
        >;
} // btl

