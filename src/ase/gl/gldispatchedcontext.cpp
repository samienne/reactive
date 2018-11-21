#include "gldispatchedcontext.h"

namespace ase
{

GlDispatchedContext::GlDispatchedContext()
{
}

void GlDispatchedContext::wait()
{
    dispatcher_.wait();
}

GlFunctions const& GlDispatchedContext::getGlFunctions() const
{
    return gl_;
}

} // namespace ase

