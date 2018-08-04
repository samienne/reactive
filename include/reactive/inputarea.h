#pragma once

#include "keyboardinput.h"
#include "pointermoveevent.h"
#include "pointerbuttonevent.h"
#include "eventresult.h"

#include <avg/obb.h>
#include <avg/transform.h>

#include <ase/hoverevent.h>
#include <ase/vector.h>

#include <btl/uniqueid.h>
#include <btl/function.h>

#include <ostream>

namespace reactive
{
    using HoverEvent = ase::HoverEvent;

    class BTL_VISIBLE InputArea
    {
    public:
        InputArea(btl::UniqueId id, avg::Obb const& obb);
        InputArea(btl::UniqueId id, std::vector<avg::Obb>&& obbs);

        InputArea(InputArea const&) = default;
        InputArea(InputArea&&) noexcept = default;

        InputArea& operator=(InputArea const&) = default;
        InputArea& operator=(InputArea&&) noexcept = default;

        btl::UniqueId getId() const;
        bool contains(avg::Vector2f pos) const;
        bool acceptsButtonEvent(PointerButtonEvent const& e) const;
        bool acceptsMoveEvent(PointerMoveEvent const& e) const;

        avg::Transform getTransform() const;
        std::vector<avg::Obb> const& getObbs() const;

        InputArea transform(avg::Transform const& t) &&;
        InputArea clip(avg::Obb const& obb) &&;
        InputArea onDown(btl::Function<EventResult (PointerButtonEvent const& e)> f) &&;
        InputArea onUp(btl::Function<EventResult (PointerButtonEvent const& e)> f) &&;
        InputArea onMove(btl::Function<EventResult (PointerMoveEvent const& e)> f) &&;
        InputArea onHover(btl::Function<void (HoverEvent const& e)> f) &&;

        EventResult emitButtonEvent(PointerButtonEvent const& e) const;
        EventResult emitMoveEvent(PointerMoveEvent const& e) const;
        void emitHoverEvent(HoverEvent const& e) const;

        std::vector<
            btl::Function<EventResult (PointerButtonEvent const& e)>
            > const& getOnDowns() const;

        std::vector<
            btl::Function<EventResult (PointerButtonEvent const& e)>
            > const& getOnUps() const;

        friend std::ostream& operator<<(std::ostream& stream,
                InputArea const& area);

    private:
        btl::UniqueId id_;
        avg::Transform transform_;
        std::vector<avg::Obb> obbs_;

        std::vector<
            btl::Function<EventResult (PointerButtonEvent const& e)>
            > onDown_;

        std::vector<
            btl::Function<EventResult (PointerButtonEvent const& e)>
            > onUp_;

        std::vector<
            btl::Function<EventResult (PointerMoveEvent const& e)>
            > onMove_;

        std::vector<
            btl::Function<void (HoverEvent const& e)>
            > onHover_;
    };

    BTL_VISIBLE auto makeInputArea(btl::UniqueId id,
            std::vector<avg::Obb>&& obbs) -> InputArea;

    BTL_VISIBLE auto makeInputArea(btl::UniqueId id, avg::Obb const& obb)
        -> InputArea;
}

