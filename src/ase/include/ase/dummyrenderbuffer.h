#pragma once

#include "renderbufferimpl.h"

namespace ase
{
    class DummyRenderBuffer : public RenderbufferImpl
    {
    public:
        DummyRenderBuffer(Format format, Vector2i size);

        Format getFormat() const override;
        Vector2i getSize() const override;

    private:
        Format format_;
        Vector2i size_;
    };
} // namespace ase

