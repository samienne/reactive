#pragma once

#include <avg/transform.h>

#include <ase/vector.h>

namespace reactive
{
    class ClickEvent
    {
    public:
        ClickEvent(unsigned int pointer, unsigned int button,
                ase::Vector2f pos);

        unsigned int getPointer() const;
        unsigned int getButton() const;
        ase::Vector2f getPos() const;

        ClickEvent transform(avg::Transform const& t) const;

        friend std::ostream& operator<<(std::ostream& stream,
                ClickEvent const& e);

    private:
        ase::Vector2f pos_;
        unsigned int pointer_;
        unsigned int button_;
    };
} // reactive

