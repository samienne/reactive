#include "keyevent.h"

namespace ase
{

KeyEvent::KeyEvent(KeyState state, KeyCode key, uint32_t modifiers,
        std::string text) :
    modifiers_(modifiers),
    key_(key),
    state_(state),
    text_(std::move(text))
{
}

bool KeyEvent::isDown() const
{
    return state_ == KeyState::down;
}

KeyState KeyEvent::getState() const
{
    return state_;
}

KeyCode KeyEvent::getKey() const
{
    return key_;
}

uint32_t KeyEvent::getModifiers() const
{
    return modifiers_;
}

bool KeyEvent::hasSymbol() const
{
    return !text_.empty();
}

std::string const& KeyEvent::getText() const
{
    return text_;
}

} // namespace

