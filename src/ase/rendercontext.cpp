#include "rendercontext.h"

#include "rendercontextimpl.h"
#include "renderqueue.h"
#include "window.h"
#include "platform.h"

#include "debug.h"

#include <stdexcept>

namespace ase
{

RenderContext::RenderContext(Platform& platform,
        std::shared_ptr<RenderContextImpl> impl) :
    deferred_(impl),
    platform_(&platform)
{
}

RenderContext::RenderContext(Platform& platform) :
    deferred_(platform.makeRenderContextImpl()),
    platform_(&platform)
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
    {
        //window.submitAll(*this);
        d()->present(window);
    }
}

void RenderContext::submit(RenderQueue&& renderQueue)
{
    if (d())
        d()->submit(std::move(renderQueue));
}

/*
void RenderContext::submit(RenderTarget& target,
        std::vector<RenderCommand>&& commands)
{
    if (d() && !commands.empty())
        d()->submit(target, std::move(commands));
}
*/

Platform& RenderContext::getPlatform() const
{
    if (!platform_)
        throw std::runtime_error("Incomplete RenderContext");

    return *platform_;
}

} // namespace

