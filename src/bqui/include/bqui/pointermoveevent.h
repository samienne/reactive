#pragma once

#include <ase/pointermoveevent.h>

#include <avg/transform.h>

namespace bqui
{
    using PointerMoveEvent = ase::PointerMoveEvent;

    PointerMoveEvent transformPointerMoveEvent(
            PointerMoveEvent const& e,
            avg::Transform const& t
            );
}

