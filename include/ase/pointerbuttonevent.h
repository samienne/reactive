#pragma once

#include "vector.h"

#include <avg/transform.h>

#include <ostream>

namespace ase
{
    enum class ButtonState
    {
        down,
        up
    };

    class PointerButtonEvent
    {
    public:
        PointerButtonEvent(unsigned int pointer, unsigned int button,
                ButtonState state, Vector2f pos);

        unsigned int getPointer() const;
        unsigned int getButton() const;
        ButtonState getState() const;
        Vector2f getPos() const;

        PointerButtonEvent transform(avg::Transform const& t) const;

        friend std::ostream& operator<<(std::ostream& stream,
                PointerButtonEvent const& e);

    private:
        Vector2f pos_;
        unsigned int pointer_;
        unsigned int button_;
        ButtonState state_;
    };
}

