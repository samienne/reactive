#pragma once

namespace ase
{
    /** @brief Outcome of presenting a surface's finished frame.
     *
     * `Ok` means the frame was presented; other values report a recoverable
     * present failure, such as a lost swapchain that must be recreated. GL
     * always reports `Ok`.
     */
    enum class PresentStatus
    {
        Ok,
    };
} // namespace ase
