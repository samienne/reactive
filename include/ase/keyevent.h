#pragma once

#include "keycode.h"
#include "keymodifier.h"

#include "asevisibility.h"

#include <string>
#include <stdint.h>

namespace ase
{
    enum class KeyState
    {
        down,
        up
    };

    class ASE_EXPORT KeyEvent
    {
    public:
        KeyEvent(KeyState state, KeyCode key, uint32_t modifiers,
                std::string text);

        bool isDown() const;
        KeyState getState() const;
        KeyCode getKey() const;
        uint32_t getModifiers() const;
        bool hasSymbol() const;
        std::string const& getText() const;

    private:
        uint32_t modifiers_;
        KeyCode key_;
        KeyState state_;
        std::string text_;
    };
}

namespace std
{
    template<>
    struct ASE_EXPORT hash<ase::KeyCode>
    {
        typedef ase::KeyCode argument_type;
        typedef std::size_t result_type;

        result_type operator()(argument_type code) const
        {
            return std::hash<unsigned int>()(static_cast<unsigned int>(code));
        }
    };
} // std

