#pragma once

#include <avg/transform.h>

#include <ase/pointerbuttonevent.h>

namespace bqui
{
    using PointerButtonEvent = ase::PointerButtonEvent;

    PointerButtonEvent transformPointerButtonEvent(
            PointerButtonEvent const& event,
            avg::Transform const& transform
            );
}

