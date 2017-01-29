#include <Eigen/Core> // This is a workaround for X.h defining Success 0

#include "glxcontext.h"

#include "glxwindow.h"
#include "glxplatform.h"

#include "debug.h"

#include <GL/glx.h>

namespace ase
{

GlxContext::GlxContext() :
    platform_(0),
    context_(0)
{
}

GlxContext::GlxContext(GlxPlatform& platform) :
    platform_(&platform),
    context_(0)
{
    context_ = platform.createGlxContext(platform.lockX());
}

GlxContext::GlxContext(GlxContext&& other) :
    platform_(other.platform_),
    context_(other.context_)
{
    other.context_ = 0;
}

GlxContext::~GlxContext()
{
    if (platform_ && context_)
        platform_->destroyGlxContext(platform_->lockX(), context_);
}

GlxContext& GlxContext::operator=(GlxContext&& other)
{
    if (platform_ && context_)
        platform_->destroyGlxContext(platform_->lockX(), context_);

    platform_ = other.platform_;
    context_ = other.context_;
    other.platform_ = 0;
    other.context_ = 0;
    return *this;
}

void GlxContext::makeCurrent(Lock const& lock, GLXDrawable drawable) const
{
    platform_->makeGlxContextCurrent(lock, context_, drawable);
}

std::ostream& operator<<(std::ostream& stream, ase::GlxContext const& context)
{
    return stream << "ase::GlxContext{ context_: " << std::hex
        << context.context_ << std::dec << " }";
}

} // namespace

