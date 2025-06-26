#include "textevent.h"

namespace ase
{

TextEvent::TextEvent(std::string text) :
    text_(std::move(text))
{
}

std::string const& TextEvent::getText() const
{
    return text_;
}

} // namespace ase

