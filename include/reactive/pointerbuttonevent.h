#pragma once

#include <avg/transform.h>

#include <ase/pointerbuttonevent.h>

namespace reactive
{
    using PointerButtonEvent = ase::PointerButtonEvent;

    PointerButtonEvent transformPointerButtonEvent(
            PointerButtonEvent const& event,
            avg::Transform const& transform
            );
} // reactive

