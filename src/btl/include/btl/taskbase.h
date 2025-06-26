#pragma once

#include "taskstatus.h"

namespace btl
{
    struct TaskBase
    {
        virtual ~TaskBase() = default;
        virtual TaskStatus run() = 0;
    };
} // btl

