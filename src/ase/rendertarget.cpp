#include "rendertarget.h"

#include "rendercontext.h"
#include "rendercommand.h"
#include "rendertargetimpl.h"
#include "platform.h"
#include "debug.h"

#include <stdexcept>

namespace ase
{

RenderTarget::RenderTarget()
{
}

RenderTarget::RenderTarget(std::shared_ptr<RenderTargetImpl>&& deferred) :
    deferred_(std::move(deferred))
{
}

RenderTarget::~RenderTarget()
{
}

bool RenderTarget::operator==(RenderTarget const& rhs) const
{
    return d() == rhs.d();
}

bool RenderTarget::operator!=(RenderTarget const& rhs) const
{
    return d() != rhs.d();
}

bool RenderTarget::operator<(RenderTarget const& rhs) const
{
    return d() > rhs.d();
}

RenderTarget::operator bool() const
{
    return d();
}

bool RenderTarget::isComplete() const
{
    if (!d())
        return false;

    return d()->isComplete();
}

Vector2i RenderTarget::getResolution() const
{
    if (!d())
        return Vector2i(0, 0);

    return d()->getResolution();
}

void RenderTarget::push(RenderCommand&& command)
{
    if (!isComplete())
        throw std::runtime_error("Incomplete render target");

    if (command.getPipeline().isBlendEnabled())
        blendedCommands_.push_back(std::move(command));
    else
        solidCommands_.push_back(std::move(command));
}

void RenderTarget::submitSolid(RenderContext& context)
{
    if (solidCommands_.empty())
        return;

    std::vector<RenderCommand> commands;
    solidCommands_.swap(commands);
    context.submit(*this, std::move(commands));
}

void RenderTarget::submitAll(RenderContext& context)
{
    submitSolid(context);

    if (blendedCommands_.empty())
        return;

    std::vector<RenderCommand> commands;
    blendedCommands_.swap(commands);
    context.submit(*this, std::move(commands));
}

} // namespace

