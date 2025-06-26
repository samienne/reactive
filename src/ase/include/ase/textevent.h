#pragma once

#include "asevisibility.h"

#include <string>

namespace ase
{
    class ASE_EXPORT TextEvent
    {
    public:
        TextEvent(std::string text);

        std::string const& getText() const;

    private:
        std::string text_;
    };
} // namespace ase

