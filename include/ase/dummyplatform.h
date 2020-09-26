#pragma once

#include "platform.h"

#include "asevisibility.h"

namespace ase
{
    class ASE_EXPORT DummyPlatform : public Platform
    {
    public:
    private:
        std::shared_ptr<RenderContextImpl> makeRenderContextImpl() override;
    };

} // namespace ase

