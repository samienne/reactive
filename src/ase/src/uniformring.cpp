#include "uniformring.h"

#include "buffer.h"
#include "commandbuffer.h"
#include "rendercontext.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace ase
{

UniformRing::UniformRing(RenderContext& context, int framesInFlight) :
    alignment_(context.getUniformBufferOffsetAlignment()),
    current_(static_cast<size_t>(std::max(framesInFlight, 1)) - 1)
{
    if (framesInFlight < 1)
        throw std::invalid_argument("UniformRing needs at least one frame");

    if (alignment_ == 0)
        throw std::invalid_argument("UniformRing needs a non-zero alignment");

    frames_.reserve(static_cast<size_t>(framesInFlight));
    for (int i = 0; i < framesInFlight; ++i)
        frames_.push_back(Frame{ context.makeUniformBuffer(), {} });
}

UniformRing::~UniformRing()
{
}

void UniformRing::beginFrame()
{
    current_ = (current_ + 1) % frames_.size();
    written_ = 0;
    open_ = true;
}

UniformBufferRange UniformRing::write(void const* data, size_t size)
{
    if (!open_)
        throw std::logic_error("UniformRing written outside a frame");

    Frame& frame = frames_[current_];
    size_t const offset = (written_ + alignment_ - 1) / alignment_ * alignment_;
    size_t const end = offset + size;

    if (frame.staging.size() < end)
        frame.staging.resize(std::max(end, frame.staging.size() * 2));

    if (size > 0)
        std::memcpy(frame.staging.data() + offset, data, size);

    written_ = end;

    return UniformBufferRange{ offset, size, frame.buffer };
}

void UniformRing::endFrame(CommandBuffer& uploads)
{
    open_ = false;

    if (written_ == 0)
        return;

    Frame const& frame = frames_[current_];
    uploads.pushUpload(frame.buffer, Buffer(frame.staging.data(), written_),
            Usage::StreamDraw);
}

size_t UniformRing::getAlignment() const
{
    return alignment_;
}

int UniformRing::getFramesInFlight() const
{
    return static_cast<int>(frames_.size());
}

} // namespace ase
