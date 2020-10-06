#include "wglplatform.h"

#include "wglrendercontext.h"

namespace ase
{

std::shared_ptr<RenderContextImpl> WglPlatform::makeRenderContextImpl()
{
    return std::make_shared<WglRenderContext>();
}

} // namespace ase

