#include "renderviewstate.h"

#include "bqui/renderview.h"

#include <ase/commandbuffer.h>
#include <ase/format.h>
#include <ase/rendercontext.h>
#include <ase/renderqueue.h>

#include <algorithm>
#include <cassert>
#include <utility>

namespace bqui
{

namespace detail
{

namespace
{
    thread_local RenderViewCollector* currentCollector = nullptr;
} // anonymous namespace

void RenderHandleCore::wake()
{
    std::vector<std::function<void()>> calls;

    {
        std::lock_guard<std::mutex> lock(mutex);
        for (auto const& waker : wakers)
        {
            if (!waker.first.expired())
                calls.push_back(waker.second);
        }
    }

    for (auto const& call : calls)
        call();
}

void RenderHandleCore::addWaker(std::weak_ptr<void const> owner,
        std::function<void()> waker)
{
    std::lock_guard<std::mutex> lock(mutex);

    wakers.erase(std::remove_if(wakers.begin(), wakers.end(),
                [&](auto const& entry)
                {
                    return entry.first.expired()
                        || (!entry.first.owner_before(owner)
                                && !owner.owner_before(entry.first));
                }),
            wakers.end());

    wakers.emplace_back(std::move(owner), std::move(waker));
}

RenderViewState::RenderViewState(std::shared_ptr<RenderHandleCore> handle) :
    handle(std::move(handle))
{
}

ase::Texture RenderViewState::prepare(ase::RenderContext& context,
        ase::Vector2i size)
{
    std::lock_guard<std::mutex> lock(mutex);

    if (targets && targets->size == size)
        return targets->texture;

    // New targets rather than reattaching: the old framebuffer may still be
    // in flight on the render thread.
    auto texture = context.makeTexture(size, ase::FORMAT_SRGBA);
    auto framebuffer = context.makeFramebuffer();
    framebuffer.setColorTarget(0, texture);
    framebuffer.setDepthTarget(
            context.makeRenderbuffer(size, ase::FORMAT_DEPTH16));

    ase::CommandBuffer commands;
    commands.pushClear(framebuffer, 0.0f, 0.0f, 0.0f, 0.0f, true, true);
    context.getMainRenderQueue().submit(std::move(commands));

    targets = Targets{ texture, std::move(framebuffer), size };
    resized = true;

    return texture;
}

std::optional<ase::Texture> RenderViewState::getTexture() const
{
    std::lock_guard<std::mutex> lock(mutex);

    if (!targets)
        return std::nullopt;

    return targets->texture;
}

RenderViewCollector::RenderViewCollector(ase::RenderContext& context) :
    context_(context),
    previous_(currentCollector)
{
    currentCollector = this;
}

RenderViewCollector::~RenderViewCollector()
{
    currentCollector = previous_;
}

RenderViewCollector* RenderViewCollector::current()
{
    return currentCollector;
}

ase::RenderContext& RenderViewCollector::getContext() const
{
    return context_;
}

void RenderViewCollector::add(std::shared_ptr<RenderViewState> view)
{
    if (std::find(views_.begin(), views_.end(), view) == views_.end())
        views_.push_back(std::move(view));
}

std::vector<std::shared_ptr<RenderViewState>> RenderViewCollector::take() &&
{
    return std::move(views_);
}

bool renderViews(ase::RenderContext& context, ase::Frame const& frame,
        std::vector<std::shared_ptr<RenderViewState>> const& views,
        std::weak_ptr<void const> wakeOwner,
        std::function<void()> const& wake)
{
    std::vector<std::shared_ptr<RenderHandleCore>> handles;
    for (auto const& view : views)
    {
        if (std::find(handles.begin(), handles.end(), view->handle)
                == handles.end())
        {
            handles.push_back(view->handle);
        }
    }

    bool again = false;

    for (auto const& handle : handles)
    {
        handle->addWaker(wakeOwner, wake);

        std::vector<RenderTarget> targets;
        bool resized = false;

        for (auto const& view : views)
        {
            if (view->handle != handle)
                continue;

            std::lock_guard<std::mutex> lock(view->mutex);
            if (!view->targets)
                continue;

            targets.push_back(RenderTarget{ RenderView(view),
                    view->targets->framebuffer, view->targets->size,
                    view->resized });
            resized = resized || view->resized;
        }

        std::shared_ptr<RenderHandle::Callback> callback;
        bool continuous = false;

        {
            std::lock_guard<std::mutex> lock(handle->mutex);
            bool requested = std::exchange(handle->frameRequested, false);
            continuous = handle->continuous;
            if (continuous || requested || resized)
                callback = handle->callback;
        }

        if (!callback || targets.empty())
            continue;

        RenderFrame renderFrame(context, frame.time, frame.dt,
                std::move(targets));

        (*callback)(renderFrame);

        for (auto const& target : renderFrame.getTargets())
        {
            auto const& state = target.view.getState();
            std::lock_guard<std::mutex> lock(state->mutex);
            state->resized = false;
        }

        if (renderFrame.isFrameRequested())
        {
            std::lock_guard<std::mutex> lock(handle->mutex);
            handle->frameRequested = true;
        }

        again = again || continuous || renderFrame.isFrameRequested();
    }

    return again;
}

} // namespace detail

RenderView::RenderView(std::shared_ptr<detail::RenderViewState> state) :
    state_(std::move(state))
{
}

bool RenderView::operator==(RenderView const& rhs) const
{
    return state_ == rhs.state_;
}

bool RenderView::operator!=(RenderView const& rhs) const
{
    return state_ != rhs.state_;
}

std::shared_ptr<detail::RenderViewState> const& RenderView::getState() const
{
    return state_;
}

RenderFrame::RenderFrame(ase::RenderContext& context,
        std::chrono::microseconds time,
        std::chrono::microseconds dt,
        std::vector<RenderTarget> targets) :
    context_(&context),
    time_(time),
    dt_(dt),
    targets_(std::move(targets))
{
}

ase::RenderContext& RenderFrame::getRenderContext() const
{
    return *context_;
}

std::chrono::microseconds RenderFrame::getTime() const
{
    return time_;
}

std::chrono::microseconds RenderFrame::getDt() const
{
    return dt_;
}

std::vector<RenderTarget> const& RenderFrame::getTargets() const
{
    return targets_;
}

void RenderFrame::requestFrame()
{
    frameRequested_ = true;
}

bool RenderFrame::isFrameRequested() const
{
    return frameRequested_;
}

RenderHandle::RenderHandle(Callback callback) :
    core_(std::make_shared<detail::RenderHandleCore>())
{
    core_->callback = std::make_shared<Callback>(std::move(callback));
}

RenderHandle& RenderHandle::operator=(RenderHandle&& rhs) noexcept
{
    if (this != &rhs)
    {
        detach();
        core_ = std::move(rhs.core_);
    }

    return *this;
}

RenderHandle::~RenderHandle()
{
    detach();
}

RenderView RenderHandle::makeView()
{
    assert(core_);
    return RenderView(std::make_shared<detail::RenderViewState>(core_));
}

void RenderHandle::requestFrame()
{
    {
        std::lock_guard<std::mutex> lock(core_->mutex);
        core_->frameRequested = true;
    }

    core_->wake();
}

void RenderHandle::setContinuous(bool continuous)
{
    {
        std::lock_guard<std::mutex> lock(core_->mutex);
        core_->continuous = continuous;
    }

    if (continuous)
        core_->wake();
}

void RenderHandle::detach()
{
    if (!core_)
        return;

    std::lock_guard<std::mutex> lock(core_->mutex);
    core_->callback.reset();
    core_->continuous = false;
}

} // namespace bqui
