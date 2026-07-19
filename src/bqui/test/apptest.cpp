#include <bqui/app.h>
#include <bqui/window.h>

#include <bqui/widget/widget.h>

#include <bq/signal/input.h>
#include <bq/signal/constant.h>

#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>

using namespace bqui;

namespace
{
    Window makeWindow(std::string title)
    {
        return window(bq::signal::constant(std::move(title)),
                widget::makeWidget());
    }

    std::pair<size_t, Window> entry(size_t id, std::string title)
    {
        return { id, makeWindow(std::move(title)) };
    }
} // namespace

// Drives the reactive window list through App::run on the headless (dummy)
// platform and asserts that WindowGlues reconcile by stable id: start with one
// window, add a second, then remove the first. Each edit is applied from the
// reconcile observer (which runs on the run-loop thread), so the sequence is
// deterministic and needs no extra threads.
TEST(App, dynamicWindowsReconcileById)
{
    auto windows = bq::signal::makeInput(
            std::vector<std::pair<size_t, Window>>{ entry(0, "first") });

    auto running = bq::signal::makeInput(true);

    std::vector<std::vector<size_t>> observed;

    int step = 0;

    App()
        .windows(windows.signal)
        .onWindowsReconciled(
            [&](std::vector<size_t> const& ids)
            {
                observed.push_back(ids);

                switch (step++)
                {
                case 0:
                    // Add a second window (id 1) alongside the first (id 0).
                    windows.handle.set(std::vector<std::pair<size_t, Window>>{
                            entry(0, "first"), entry(1, "second") });
                    break;
                case 1:
                    // Remove the first window; only id 1 should remain.
                    windows.handle.set(std::vector<std::pair<size_t, Window>>{
                            entry(1, "second") });
                    break;
                default:
                    // Reconciliation done; stop the loop.
                    running.handle.set(false);
                    break;
                }
            })
        .run(running.signal);

    ASSERT_EQ(observed.size(), 3u);
    EXPECT_EQ(observed[0], (std::vector<size_t>{ 0 }));
    EXPECT_EQ(observed[1], (std::vector<size_t>{ 0, 1 }));
    EXPECT_EQ(observed[2], (std::vector<size_t>{ 1 }));
}

// A fixed window set (the initializer_list convenience) enumerates windows with
// contiguous ids starting at zero and reconciles them once at startup.
TEST(App, staticWindowsEnumerateFromZero)
{
    auto running = bq::signal::makeInput(true);

    std::vector<size_t> observed;

    App()
        .windows({ makeWindow("a"), makeWindow("b"), makeWindow("c") })
        .onWindowsReconciled(
            [&](std::vector<size_t> const& ids)
            {
                observed = ids;
                running.handle.set(false);
            })
        .run(running.signal);

    EXPECT_EQ(observed, (std::vector<size_t>{ 0, 1, 2 }));
}
