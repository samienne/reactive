#include <bqui/app.h>
#include <bqui/window.h>

#include <bqui/widget/widget.h>

#include <bq/signal/constant.h>
#include <bq/signal/frameinfo.h>
#include <bq/signal/signalcontext.h>

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

using namespace bqui;

// The app's window collection, without running the app. Everything here is
// true of an App that has never opened a window, which is what lets these
// tests run on a backend that has no display; App::run itself is covered in
// apptest.cpp, where the backend is headless.

namespace
{
    Window makeWindow(std::string title)
    {
        return window(bq::signal::constant(std::move(title)),
                widget::makeWidget());
    }

    std::vector<std::string> titles(std::vector<Window> const& windows)
    {
        std::vector<std::string> result;

        for (auto const& window : windows)
        {
            result.push_back(bq::signal::makeSignalContext(window.getTitle())
                    .evaluate<0>().get<0>());
        }

        return result;
    }
} // namespace

TEST(Window, copiesAreOneWindow)
{
    Window w = makeWindow("a");
    Window copy = w;

    EXPECT_EQ(w.getId(), copy.getId());
    EXPECT_NE(w.getId(), makeWindow("a").getId());
}

TEST(App, addWindowsAppends)
{
    App app;

    app.addWindow(makeWindow("a"));
    app.addWindows({ makeWindow("b"), makeWindow("c") });

    EXPECT_EQ((std::vector<std::string>{ "a", "b", "c" }),
            titles(app.getWindows()));
}

TEST(App, removeWindowRemovesByIdentity)
{
    App app;

    Window a = makeWindow("a");
    Window b = makeWindow("b");
    app.addWindows({ a, b });

    app.removeWindow(a.getId());

    EXPECT_EQ(std::vector<std::string>{ "b" }, titles(app.getWindows()));
}

TEST(App, removingAWindowThatIsNotThereDoesNothing)
{
    App app;
    app.addWindow(makeWindow("a"));

    app.removeWindow(makeWindow("b").getId());

    EXPECT_EQ(std::vector<std::string>{ "a" }, titles(app.getWindows()));
}

TEST(App, aWindowClosesItself)
{
    App app;

    Window a = makeWindow("a");
    Window b = makeWindow("b");
    app.addWindows({ a, b });

    a.close();

    EXPECT_EQ(std::vector<std::string>{ "b" }, titles(app.getWindows()));

    // Closing twice is a race a UI can lose, so the second close is not an
    // error.
    a.close();

    EXPECT_EQ(std::vector<std::string>{ "b" }, titles(app.getWindows()));
}

// The handle can be minted before the window, which is what lets a widget
// inside the window close it: a window that captured itself would own the
// widget that owns it.
TEST(App, aWidgetInsideTheWindowCanCloseIt)
{
    App app;

    WindowHandle handle;
    std::function<void()> closeFromTheWidget = [handle]() { handle.close(); };

    app.addWindow(window(bq::signal::constant<std::string>("a"),
                widget::makeWidget(), handle));

    ASSERT_EQ(1u, app.getWindows().size());
    EXPECT_EQ(handle.getId(), app.getWindows()[0].getId());

    closeFromTheWidget();

    EXPECT_TRUE(app.getWindows().empty());
}

TEST(App, closingAWindowThatWasNeverAddedDoesNothing)
{
    Window a = makeWindow("a");

    a.close();

    App app;
    app.addWindow(a);

    EXPECT_EQ(std::vector<std::string>{ "a" }, titles(app.getWindows()));
}

// A window outlives its app whenever a widget or a callback still names it,
// and the app is what close() reaches. It is held weakly for exactly this.
TEST(App, closingAWindowWhoseAppIsGoneDoesNothing)
{
    std::optional<App> app;
    app.emplace();

    Window a = makeWindow("a");
    app->addWindow(a);

    app.reset();

    a.close();
}

TEST(App, aWindowCannotBeOpenedTwice)
{
    App app;

    Window a = makeWindow("a");
    app.addWindow(a);

    EXPECT_THROW(app.addWindow(a), std::invalid_argument);
    EXPECT_EQ(std::vector<std::string>{ "a" }, titles(app.getWindows()));
}

TEST(App, aWindowCannotBeOpenedTwiceWithinOneCall)
{
    App app;

    Window a = makeWindow("a");

    EXPECT_THROW(app.addWindows({ a, makeWindow("b"), a }),
            std::invalid_argument);

    // The batch was rejected whole, so the window that was fine is not open
    // either.
    EXPECT_TRUE(app.getWindows().empty());
}

TEST(App, theWindowSignalFollowsTheCollection)
{
    App app;
    app.addWindow(makeWindow("a"));

    auto c = bq::signal::makeSignalContext(app.getWindowsSignal());

    EXPECT_EQ(std::vector<std::string>{ "a" }, titles(c.evaluate<0>().get<0>()));

    app.addWindow(makeWindow("b"));
    c.update(bq::signal::FrameInfo(1, {}));

    EXPECT_EQ((std::vector<std::string>{ "a", "b" }),
            titles(c.evaluate<0>().get<0>()));
}

// Copies of an App are one app: it is a handle to the state, which is what
// lets app() hand the same one out everywhere.
TEST(App, copiesShareTheCollection)
{
    App app;
    App copy = app;

    copy.addWindow(makeWindow("a"));

    EXPECT_EQ(std::vector<std::string>{ "a" }, titles(app.getWindows()));
}
