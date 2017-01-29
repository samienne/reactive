#pragma once

#include "futurebase.h"

#include <utility>

namespace btl
{
    namespace future
    {
        class FutureConnection
        {
        public:
            FutureConnection(std::shared_ptr<FutureBase> control) :
                control_(std::move(control))
            {
            }

        private:
            std::shared_ptr<FutureBase> control_;
        };
    } // future
} // btl

