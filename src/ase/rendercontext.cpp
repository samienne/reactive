#include "rendercontext.h"

#include "rendercontextimpl.h"
#include "commandbuffer.h"
#include "window.h"
#include "platform.h"

#include "debug.h"

#include <stdexcept>

namespace ase
{

RenderContext::RenderContext(Platform& platform,
        std::shared_ptr<RenderContextImpl> impl) :
    deferred_(impl),
    platform_(platform)
{
}

RenderContext::RenderContext(Platform& platform) :
    deferred_(platform.makeRenderContextImpl()),
    platform_(platform)
{
}

RenderContext::~RenderContext()
{
}

void RenderContext::flush()
{
    if (d())
        d()->flush();
}

void RenderContext::finish()
{
    if (d())
        d()->finish();
}

void RenderContext::present(Window& window)
{
    if (d())
        d()->present(window);
}

void RenderContext::submit(CommandBuffer&& commands)
{
    if (d())
        d()->submit(std::move(commands));
}

Platform& RenderContext::getPlatform() const
{
    return platform_;
}

} // namespace

