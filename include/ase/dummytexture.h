#pragma once

#include "textureimpl.h"

namespace ase
{
    class DummyTexture : public TextureImpl
    {
    public:
        DummyTexture(Vector2i size, Format format);
        void setSize(Vector2i size) override;
        void setFormat(Format format) override;
        Vector2i getSize() const override;
        Format getFormat() const override;

    private:
        Vector2i size_ = Vector2i(0, 0);
        Format format_ = Format::FORMAT_UNKNOWN;
    };
} // namespace ase

