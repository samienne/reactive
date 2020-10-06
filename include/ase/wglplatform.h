#pragma once

#include "platform.h"

#include "asevisibility.h"

namespace ase
{
    class WglPlatform : public Platform
    {
    public:
    private:
        std::shared_ptr<RenderContextImpl> makeRenderContextImpl() override;
    }; // WglPlatform
} // namespace

