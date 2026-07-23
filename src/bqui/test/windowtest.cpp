#include <bqui/app.h>
#include <bqui/window.h>

#include <bqui/widget/widget.h>

#include <bq/signal/constant.h>
#include <bq/signal/frameinfo.h>
#include <bq/signal/signalcontext.h>

#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

using namespace bqui;

// The app's window collection, without running the app. A window is a handle
// over its own persistent data, and everything here is true of that data and
// of the collection an App holds it in — which is why these tests need no
// display. App::run itself is covered in apptest.cpp, where the backend is
// headless.
//
// A window's widget is mount-time content supplied to addWindow; it is not part
// of the window and these tests never run it, so they pass an empty one.

namespace
{
    Window makeWindow(std::string title)
    {
        return window(bq::signal::constant(std::move(title)));
    }

    void add(App& app, Window window)
    {
        app.addWindow(std::move(window), widget::makeWidget());
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

TEST(AppWindows, copiesAreOneWindow)
{
    Window w = makeWindow("a");
    Window copy = w;

    EXPECT_EQ(w.getId(), copy.getId());
    EXPECT_NE(w.getId(), makeWindow("a").getId());
}

TEST(AppWindows, addWindowAppends)
{
    App app;

    add(app, makeWindow("a"));
    add(app, makeWindow("b"));
    add(app, makeWindow("c"));

    EXPECT_EQ((std::vector<std::string>{ "a", "b", "c" }),
            titles(app.getWindows()));
}

TEST(AppWindows, removeWindowRemovesByIdentity)
{
    App app;

    Window a = makeWindow("a");
    Window b = makeWindow("b");
    add(app, a);
    add(app, b);

    app.removeWindow(a.getId());

    EXPECT_EQ(std::vector<std::string>{ "b" }, titles(app.getWindows()));
}

TEST(AppWindows, removingAWindowThatIsNotThereDoesNothing)
{
    App app;
    add(app, makeWindow("a"));

    app.removeWindow(makeWindow("b").getId());

    EXPECT_EQ(std::vector<std::string>{ "a" }, titles(app.getWindows()));
}

TEST(AppWindows, aWindowClosesItself)
{
    App app;

    Window a = makeWindow("a");
    Window b = makeWindow("b");
    add(app, a);
    add(app, b);

    a.close();

    EXPECT_EQ(std::vector<std::string>{ "b" }, titles(app.getWindows()));

    // Closing twice is a race a UI can lose, so the second close is not an
    // error.
    a.close();

    EXPECT_EQ(std::vector<std::string>{ "b" }, titles(app.getWindows()));
}

// A widget inside the window may hold the very Window that owns it and close it
// through that. A window is a small handle now, so this closes no cycle: the
// app owns the widget, the widget owns the window, and the window points back
// at the app only weakly.
TEST(AppWindows, aWidgetInsideTheWindowCanCloseIt)
{
    App app;

    Window w = makeWindow("a");
    std::function<void()> closeFromTheWidget = [w]() { w.close(); };

    app.addWindow(w, widget::makeWidget());

    ASSERT_EQ(1u, app.getWindows().size());
    EXPECT_EQ(w.getId(), app.getWindows()[0].getId());

    closeFromTheWidget();

    EXPECT_TRUE(app.getWindows().empty());
}

TEST(AppWindows, closingAWindowThatWasNeverAddedDoesNothing)
{
    Window a = makeWindow("a");

    a.close();

    App app;
    add(app, a);

    EXPECT_EQ(std::vector<std::string>{ "a" }, titles(app.getWindows()));
}

// A window outlives its app whenever a widget or a callback still names it,
// and the app is what close() reaches. It is held weakly for exactly this.
TEST(AppWindows, closingAWindowWhoseAppIsGoneDoesNothing)
{
    std::optional<App> app;
    app.emplace();

    Window a = makeWindow("a");
    add(*app, a);

    app.reset();

    a.close();
}

TEST(AppWindows, aWindowCannotBeOpenedTwice)
{
    App app;

    Window a = makeWindow("a");
    add(app, a);

    EXPECT_THROW(add(app, a), std::invalid_argument);
    EXPECT_EQ(std::vector<std::string>{ "a" }, titles(app.getWindows()));
}

// A window belongs to one app, because close() has to know which app to leave.
// Being open in one is what makes it unavailable, not having ever been added.
TEST(AppWindows, aWindowCannotBeOpenInTwoApps)
{
    App first;
    App second;

    Window a = makeWindow("a");
    add(first, a);

    EXPECT_THROW(add(second, a), std::invalid_argument);
    EXPECT_TRUE(second.getWindows().empty());

    first.removeWindow(a.getId());

    add(second, a);

    EXPECT_EQ(std::vector<std::string>{ "a" }, titles(second.getWindows()));

    // The window follows: it is the second app it closes out of now.
    a.close();

    EXPECT_TRUE(second.getWindows().empty());
}

TEST(AppWindows, aWindowThatWasClosedCanBeOpenedAgain)
{
    App app;

    Window a = makeWindow("a");
    add(app, a);
    a.close();

    add(app, a);

    EXPECT_EQ(std::vector<std::string>{ "a" }, titles(app.getWindows()));
}

TEST(AppWindows, theWindowSignalFollowsTheCollection)
{
    App app;
    add(app, makeWindow("a"));

    auto c = bq::signal::makeSignalContext(app.getWindowsSignal());

    EXPECT_EQ(std::vector<std::string>{ "a" },
            titles(c.evaluate<0>().get<0>()));

    add(app, makeWindow("b"));
    c.update(bq::signal::FrameInfo(1, {}));

    EXPECT_EQ((std::vector<std::string>{ "a", "b" }),
            titles(c.evaluate<0>().get<0>()));
}

// Copies of an App are one app: it is a handle to the state, which is what
// lets app() hand the same one out everywhere.
TEST(AppWindows, copiesShareTheCollection)
{
    App app;
    App copy = app;

    add(copy, makeWindow("a"));

    EXPECT_EQ(std::vector<std::string>{ "a" }, titles(app.getWindows()));
}
