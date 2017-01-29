#pragma once

#include "keyboardinput.h"

#include <avg/obb.h>
#include <avg/transform.h>

#include <ase/pointerbuttonevent.h>
#include <ase/vector.h>

#include <functional>
#include <ostream>

namespace reactive
{
    class InputArea
    {
    public:
        InputArea(avg::Obb const& obb);
        InputArea(std::vector<avg::Obb>&& obbs);

        InputArea(InputArea const&) = default;
        InputArea(InputArea&&) = default;

        InputArea& operator=(InputArea const&) = default;
        InputArea& operator=(InputArea&&) = default;

        bool contains(avg::Vector2f pos) const;

        avg::Transform getTransform() const;
        std::vector<avg::Obb> const& getObbs() const;

        InputArea transform(avg::Transform const& t) &&;
        InputArea clip(avg::Obb const& obb) &&;
        InputArea onDown(std::function<void (ase::PointerButtonEvent const& e)>
                const& f) &&;
        InputArea onUp(std::function<void (ase::PointerButtonEvent const& e)>
                const& f) &&;

        void emit(ase::PointerButtonEvent const& e) const;

        std::vector<
            std::function<void (ase::PointerButtonEvent const& e)>
            > const& getOnDowns() const;

        std::vector<
            std::function<void (ase::PointerButtonEvent const& e)>
            > const& getOnUps() const;

        friend std::ostream& operator<<(std::ostream& stream,
                InputArea const& area);

    private:
        avg::Transform transform_;
        std::vector<avg::Obb> obbs_;
        std::vector<
            std::function<void (ase::PointerButtonEvent const& e)>
            > onDown_;
        std::vector<
            std::function<void (ase::PointerButtonEvent const& e)>
            > onUp_;
        btl::option<KeyboardInput> keyboard_;
    };

    auto makeInputArea(std::vector<avg::Obb>&& obbs)
        -> InputArea;

    auto makeInputArea(avg::Obb const& obb)
        -> InputArea;
}

