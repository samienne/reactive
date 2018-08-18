#pragma once

namespace reactive
{
    enum class EventResult
    {
        // Gesture recognized and finished, no need to get further events.
        finish,

        // The gesture is possible but not finished, send more events.
        possible,

        // We have detected gesture and we use any events available.
        // Cancel others.
        exclusive,

        // Gesture not possible, no need to get further events
        reject
    };
} // namespace reactive

