#include "attachmentframebuffer.h"

namespace ase
{

AttachmentFramebuffer::AttachmentFramebuffer() :
    id_(btl::makeUniqueId()),
    generation_(0)
{
}

btl::UniqueId AttachmentFramebuffer::getId() const noexcept
{
    return id_;
}

uint64_t AttachmentFramebuffer::getGeneration() const noexcept
{
    return generation_.load(std::memory_order_acquire);
}

AttachmentFramebuffer::Snapshot AttachmentFramebuffer::snapshot() const
{
    std::unique_lock<std::mutex> lock(mutex_);
    return Snapshot{
        generation_.load(std::memory_order_relaxed),
        attachments_
    };
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
    updateSize();
    generation_.fetch_add(1, std::memory_order_release);
}

void AttachmentFramebuffer::unsetColorTarget(size_t index)
{
    std::unique_lock<std::mutex> lock(mutex_);
    attachments_.color.at(index).reset();
    updateSize();
    generation_.fetch_add(1, std::memory_order_release);
}

void AttachmentFramebuffer::setDepthTarget(Renderbuffer buffer)
{
    std::unique_lock<std::mutex> lock(mutex_);
    attachments_.depth.emplace(std::move(buffer));
    generation_.fetch_add(1, std::memory_order_release);
}

void AttachmentFramebuffer::unsetDepthTarget()
{
    std::unique_lock<std::mutex> lock(mutex_);
    attachments_.depth.reset();
    generation_.fetch_add(1, std::memory_order_release);
}

void AttachmentFramebuffer::updateSize()
{
    if (attachments_.color[0].has_value())
        attachments_.size = attachments_.color[0]->getSize();
    else
        attachments_.size = Vector2i(0, 0);
}

} // namespace ase
