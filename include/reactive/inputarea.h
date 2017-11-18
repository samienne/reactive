#pragma once

#include "keyboardinput.h"

#include <avg/obb.h>
#include <avg/transform.h>

#include <ase/pointerbuttonevent.h>
#include <ase/vector.h>

#include <btl/function.h>

#include <ostream>

namespace reactive
{
    class InputArea
    {
    public:
        InputArea(avg::Obb const& obb);
        InputArea(std::vector<avg::Obb>&& obbs);

        InputArea(InputArea const&) = default;
        InputArea(InputArea&&) noexcept = default;

        InputArea& operator=(InputArea const&) = default;
        InputArea& operator=(InputArea&&) noexcept = default;

        bool contains(avg::Vector2f pos) const;

        avg::Transform getTransform() const;
        std::vector<avg::Obb> const& getObbs() const;

        InputArea transform(avg::Transform const& t) &&;
        InputArea clip(avg::Obb const& obb) &&;
        InputArea onDown(btl::Function<void (ase::PointerButtonEvent const& e)>
                const& f) &&;
        InputArea onUp(btl::Function<void (ase::PointerButtonEvent const& e)>
                const& f) &&;

        void emit(ase::PointerButtonEvent const& e) const;

        std::vector<
            btl::Function<void (ase::PointerButtonEvent const& e)>
            > const& getOnDowns() const;

        std::vector<
            btl::Function<void (ase::PointerButtonEvent const& e)>
            > const& getOnUps() const;

        friend std::ostream& operator<<(std::ostream& stream,
                InputArea const& area);

    private:
        avg::Transform transform_;
        std::vector<avg::Obb> obbs_;
        std::vector<
            btl::Function<void (ase::PointerButtonEvent const& e)>
            > onDown_;
        std::vector<
            btl::Function<void (ase::PointerButtonEvent const& e)>
            > onUp_;
        btl::option<KeyboardInput> keyboard_;
    };

    auto makeInputArea(std::vector<avg::Obb>&& obbs)
        -> InputArea;

    auto makeInputArea(avg::Obb const& obb)
        -> InputArea;
}

