#pragma once

#include "textureimpl.h"

namespace ase
{
    class DummyTexture : public TextureImpl
    {
    public:
        Vector2i getSize() const override;
    };
} // namespace ase

