#ifndef BG_ATOMICREFCOUNTABLE_H
#define BG_ATOMICREFCOUNTABLE_H

#include "asevisibility.h"

#include <atomic>

namespace Bg
{
    class ASE_EXPORT AtomicRefCountable
    {
    public:
        AtomicRefCountable() :
            refCount_(1)
        {
        }

        virtual ~AtomicRefCountable()
        {
        }

        void ref()
        {
            refCount_ += 1;
        }

        void unref()
        {
            int refCount = refCount_ -= 1;
            if (refCount == 0)
                delete this;
        }

        int getRefCount() const
        {
            return refCount_;
        }

    private:
        AtomicRefCountable(AtomicRefCountable const& rhs);
        void operator=(AtomicRefCountable const& rhs);

        std::atomic<int> refCount_;
    };
}

#endif // BG_ATOMICREFCOUNTABLE_H

