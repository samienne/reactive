#include "fence.h"

#include "fenceimpl.h"

namespace ase
{

Fence::Fence(std::shared_ptr<FenceImpl> impl) :
    deferred_(std::move(impl))
{
}

bool Fence::isSignaled() const
{
    return d()->isSignaled();
}

btl::future::Future<> Fence::whenSignaled()
{
    return d()->whenSignaled();
}

} // namespace ase
