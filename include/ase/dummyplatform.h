#pragma once

#include "platform.h"

namespace ase
{
    class BTL_VISIBLE DummyPlatform : public Platform
    {
    public:
    private:
        std::shared_ptr<RenderContextImpl> makeRenderContextImpl() override;
    };

} // namespace ase

