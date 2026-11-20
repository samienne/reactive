#include "attachmentframebuffer.h"

namespace ase
{

namespace
{
    std::atomic<uint64_t> nextId(1);
} // anonymous namespace

AttachmentFramebuffer::AttachmentFramebuffer() :
    id_(nextId.fetch_add(1, std::memory_order_relaxed)),
    version_(0)
{
}

uint64_t AttachmentFramebuffer::getId() const noexcept
{
    return id_;
}

uint64_t AttachmentFramebuffer::getVersion() const noexcept
{
    return version_.load(std::memory_order_acquire);
}

AttachmentFramebuffer::Snapshot AttachmentFramebuffer::snapshot() const
{
    std::unique_lock<std::mutex> lock(mutex_);
    return Snapshot{ version_.load(std::memory_order_relaxed), attachments_ };
}

AttachmentFramebuffer const* AttachmentFramebuffer::asAttachments()
    const noexcept
{
    return this;
}

void AttachmentFramebuffer::setColorTarget(size_t index, Texture texture)
{
    std::unique_lock<std::mutex> lock(mutex_);
    attachments_.color.at(index).emplace(std::move(texture));
    attachments_.colorBuffers.at(index).reset();
    updateSize();
    version_.fetch_add(1, std::memory_order_release);
}

void AttachmentFramebuffer::setColorTarget(size_t index, Renderbuffer buffer)
{
    std::unique_lock<std::mutex> lock(mutex_);
    attachments_.colorBuffers.at(index).emplace(std::move(buffer));
    attachments_.color.at(index).reset();
    updateSize();
    version_.fetch_add(1, std::memory_order_release);
}

void AttachmentFramebuffer::unsetColorTarget(size_t index)
{
    std::unique_lock<std::mutex> lock(mutex_);
    attachments_.color.at(index).reset();
    attachments_.colorBuffers.at(index).reset();
    updateSize();
    version_.fetch_add(1, std::memory_order_release);
}

void AttachmentFramebuffer::setDepthTarget(Renderbuffer buffer)
{
    std::unique_lock<std::mutex> lock(mutex_);
    attachments_.depth.emplace(std::move(buffer));
    version_.fetch_add(1, std::memory_order_release);
}

void AttachmentFramebuffer::unsetDepthTarget()
{
    std::unique_lock<std::mutex> lock(mutex_);
    attachments_.depth.reset();
    version_.fetch_add(1, std::memory_order_release);
}

void AttachmentFramebuffer::setStencilTarget(Renderbuffer /*buffer*/)
{
    // No backend attaches a separate stencil target; it is ignored.
}

void AttachmentFramebuffer::unsetStencilTarget()
{
}

Format AttachmentFramebuffer::getColorFormat() const
{
    std::unique_lock<std::mutex> lock(mutex_);
    if (attachments_.color[0].has_value())
        return attachments_.color[0]->getFormat();
    if (attachments_.colorBuffers[0].has_value())
        return attachments_.colorBuffers[0]->getFormat();
    return FORMAT_UNKNOWN;
}

std::optional<Format> AttachmentFramebuffer::getDepthFormat() const
{
    std::unique_lock<std::mutex> lock(mutex_);
    if (attachments_.depth.has_value())
        return attachments_.depth->getFormat();
    return std::nullopt;
}

void AttachmentFramebuffer::updateSize()
{
    if (attachments_.color[0].has_value())
        attachments_.size = attachments_.color[0]->getSize();
    else if (attachments_.colorBuffers[0].has_value())
        attachments_.size = attachments_.colorBuffers[0]->getSize();
    else
        attachments_.size = Vector2i(0, 0);
}

} // namespace ase
