#pragma once

namespace ase
{
    /** @brief Outcome of presenting a surface's finished frame. GL always
     * reports `Ok`; a backend whose present can fail — a lost swapchain that
     * must be recreated — reports it here, so present never has to be retrofitted
     * from `void` to a status at every call site. */
    enum class PresentStatus
    {
        Ok,
    };
} // namespace ase
