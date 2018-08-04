#pragma once

namespace reactive
{
    enum class EventResult
    {
        accept, // This event finished a gesture/click. Send cancel event to others.
        possible, // This is a part of a gesture/click, keep them coming
        reject  // No use for this event, find some other handler for it
    };
} // namespace reactive

