#include "bqui/widget/stack.h"

#include "bqui/widget/layout.h"

#include "bqui/stacksizehint.h"
#include "bqui/sizehint.h"

namespace bqui::widget
{
    AnyWidget stack(std::vector<AnyWidget> widgets)
    {
        auto obbMap = [](ase::Vector2f size,
                std::vector<SizeHint> const& hints)
            -> std::vector<avg::Obb>
        {
            std::vector<avg::Obb> obbs;
            obbs.reserve(hints.size());
            auto obb = avg::Obb(size);
            for (size_t i = 0; i < hints.size(); ++i)
                obbs.push_back(obb);

            return obbs;
        };

        return layout(stackSizeHints, obbMap, std::move(widgets));
    }
}

