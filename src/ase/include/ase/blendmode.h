#pragma once

#include <btl/visibility.h>

namespace ase
{
    enum class BlendMode
    {
        Zero,
        One,
        SrcColor,
        OneMinusSrcColor,
        DstColor,
        OneMinusDstColor,
        SrcAlpha,
        OneMinusSrcAlpha,
        DstAlpha,
        OneMinusDstAlpha,
        ConstantColor,
        OneMinusConstantColor,
        ConstantAlpha,
        OneMinusConstantAlpha
    };

    /**
     * @brief The source and destination factors of a blended pipeline.
     *
     * `BlendFunc{ BlendMode::One, BlendMode::OneMinusSrcAlpha }` is
     * premultiplied over.
     */
    struct BlendFunc
    {
        BlendMode srcFactor;
        BlendMode dstFactor;
    };
}

