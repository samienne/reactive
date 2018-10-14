#pragma once

#include <btl/visibility.h>

#include <vector>

namespace ase
{
    class RenderCommand;

    class BTL_VISIBLE RenderQueue
    {
    public:
        void push(RenderCommand&& command);

    private:
        friend class RenderContext;

        std::vector<RenderCommand> commands_;
    };
} // namespace ase

