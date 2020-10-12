#pragma once

#include "platformimpl.h"

#include "asevisibility.h"

#include <windows.h>

namespace ase
{
    class ASE_EXPORT WindowsPlatform : public PlatformImpl
    {
    public:
        WindowsPlatform();

        WindowsPlatform(WindowsPlatform const&) = delete;
        WindowsPlatform& operator=(WindowsPlatform const&) = delete;

    private:
        // From PlatformImpl
        Window makeWindow(Vector2i size) override;
        void handleEvents() override;
        RenderContext makeRenderContext() override;

    private:
    };

} // namespace ase

