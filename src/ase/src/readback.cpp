#include "readback.h"

namespace ase
{

ReadbackFailure::ReadbackFailure(ReadbackError error) :
    error_(error)
{
}

ReadbackError ReadbackFailure::error() const noexcept
{
    return error_;
}

char const* ReadbackFailure::what() const noexcept
{
    switch (error_)
    {
    case ReadbackError::Unsupported:
        return "readback unsupported";
    case ReadbackError::Failed:
        return "readback failed";
    }

    return "readback failed";
}

} // namespace ase
