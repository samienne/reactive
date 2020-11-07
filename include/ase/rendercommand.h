#pragma once

#include "drawcommand.h"

#include <variant>

namespace ase
{
    using RenderCommand = std::variant<DrawCommand>;

} // namespace ase
