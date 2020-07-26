#include "dummyplatform.h"

#include "dummyrendercontext.h"

namespace ase
{

std::shared_ptr<RenderContextImpl> DummyPlatform::makeRenderContextImpl()
{
    return std::make_shared<DummyRenderContext>();
}

} // namespace ase

