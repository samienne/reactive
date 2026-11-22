#include "remote/dispatcher.h"

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace bqui::remote;

using nlohmann::json;

namespace
{
    constexpr int kTestError = -32001;

    std::string request(int id, std::string const& method)
    {
        json message = {
            { "jsonrpc", "2.0" },
            { "id", id },
            { "method", method },
        };
        return message.dump();
    }

    // A dispatcher over a recording sink, with one method per way of
    // answering. `effects` lists the handlers in the order they ran.
    struct DispatcherFixture
    {
        std::vector<json> sent;
        std::vector<std::string> effects;
        std::vector<Responder> kept;
        std::vector<DispatchGate> gates;
        std::unique_ptr<Dispatcher> dispatcher;

        DispatcherFixture()
        {
            // A handler appends to these while a call on an element runs.
            kept.reserve(8);
            gates.reserve(8);

            dispatcher = std::make_unique<Dispatcher>(
                    [this](std::string const& frame)
                    {
                        sent.push_back(json::parse(frame));
                    });

            dispatcher->add({ "fast", "", {},
                [this](json const&, Responder&)
                {
                    effects.push_back("fast");
                    return json{ { "from", "fast" } };
                } });

            dispatcher->add({ "slow", "", {},
                [this](json const&, Responder& responder) -> Reply
                {
                    effects.push_back("slow");
                    kept.push_back(std::move(responder));
                    return pending;
                } });

            dispatcher->add({ "gated", "", {},
                [this](json const&, Responder& responder) -> Reply
                {
                    effects.push_back("gated");
                    kept.push_back(std::move(responder));
                    gates.push_back(dispatcher->holdGate());
                    return pending;
                } });

            dispatcher->add({ "lost", "", {},
                [](json const&, Responder&) -> Reply
                {
                    return pending;
                } });

            dispatcher->add({ "fail", "", {},
                [](json const&, Responder&) -> Reply
                {
                    throw RpcError{ kTestError, "failed at once" };
                } });

            dispatcher->add({ "keptAndReturned", "", {},
                [this](json const&, Responder& responder) -> Reply
                {
                    kept.push_back(std::move(responder));
                    return json("returned");
                } });
        }

        std::vector<int> sentIds() const
        {
            std::vector<int> ids;
            for (auto const& frame : sent)
                ids.push_back(frame.at("id").get<int>());
            return ids;
        }
    };
} // namespace

TEST(dispatcher, deferredReplyComesLaterWhileOtherRequestsAreServed)
{
    DispatcherFixture f;

    f.dispatcher->dispatch(request(1, "slow"));
    f.dispatcher->dispatch(request(2, "fast"));
    f.dispatcher->dispatch(request(3, "fast"));

    EXPECT_EQ((std::vector<int>{ 2, 3 }), f.sentIds());
    EXPECT_EQ((std::vector<std::string>{ "slow", "fast", "fast" }), f.effects);

    ASSERT_EQ(1u, f.kept.size());
    f.kept[0].resolve({ { "from", "slow" } });

    ASSERT_EQ((std::vector<int>{ 2, 3, 1 }), f.sentIds());
    EXPECT_EQ("fast", f.sent[0].at("result").at("from"));
    EXPECT_EQ("slow", f.sent[2].at("result").at("from"));
}

TEST(dispatcher, overtakingRepliesCarryTheirOwnIds)
{
    DispatcherFixture f;

    f.dispatcher->dispatch(request(10, "slow"));
    f.dispatcher->dispatch(request(11, "slow"));
    f.dispatcher->dispatch(request(12, "fast"));

    ASSERT_EQ(2u, f.kept.size());
    f.kept[1].resolve("second");
    f.kept[0].resolve("first");

    ASSERT_EQ((std::vector<int>{ 12, 11, 10 }), f.sentIds());
    EXPECT_EQ("second", f.sent[1].at("result"));
    EXPECT_EQ("first", f.sent[2].at("result"));
}

TEST(dispatcher, deferredHandlerCanAnswerWithAnError)
{
    DispatcherFixture f;

    f.dispatcher->dispatch(request(1, "slow"));
    ASSERT_EQ(1u, f.kept.size());
    f.kept[0].reject({ kTestError, "failed later", { { "why", "test" } } });

    ASSERT_EQ(1u, f.sent.size());
    EXPECT_EQ(1, f.sent[0].at("id"));
    EXPECT_FALSE(f.sent[0].contains("result"));
    EXPECT_EQ(kTestError, f.sent[0].at("error").at("code"));
    EXPECT_EQ("failed later", f.sent[0].at("error").at("message"));
    EXPECT_EQ("test", f.sent[0].at("error").at("data").at("why"));
}

TEST(dispatcher, thrownErrorIsTheOnlyAnswer)
{
    DispatcherFixture f;

    f.dispatcher->dispatch(request(1, "fail"));

    ASSERT_EQ(1u, f.sent.size());
    EXPECT_EQ(kTestError, f.sent[0].at("error").at("code"));
}

TEST(dispatcher, onlyTheFirstAnswerCounts)
{
    DispatcherFixture f;

    f.dispatcher->dispatch(request(1, "slow"));
    f.kept[0].resolve("once");
    f.kept[0].resolve("twice");
    f.kept[0].reject({ kTestError, "thrice" });

    f.dispatcher->dispatch(request(2, "keptAndReturned"));
    f.kept[1].resolve("kept");
    f.kept.clear();

    ASSERT_EQ((std::vector<int>{ 1, 2 }), f.sentIds());
    EXPECT_EQ("once", f.sent[0].at("result"));
    EXPECT_EQ("returned", f.sent[1].at("result"));
}

TEST(dispatcher, droppedResponderAnswersWithAnInternalError)
{
    DispatcherFixture f;

    f.dispatcher->dispatch(request(1, "lost"));
    ASSERT_EQ(1u, f.sent.size());
    EXPECT_EQ(kInternalError, f.sent[0].at("error").at("code"));

    f.dispatcher->dispatch(request(2, "slow"));
    ASSERT_EQ(1u, f.sent.size());
    f.kept.clear();

    ASSERT_EQ((std::vector<int>{ 1, 2 }), f.sentIds());
    EXPECT_EQ(kInternalError, f.sent[1].at("error").at("code"));
}

TEST(dispatcher, deferredNotificationIsNeverAnswered)
{
    DispatcherFixture f;

    f.dispatcher->dispatch(
            json{ { "jsonrpc", "2.0" }, { "method", "slow" } }.dump());
    f.dispatcher->dispatch(
            json{ { "jsonrpc", "2.0" }, { "method", "slow" } }.dump());

    ASSERT_EQ(2u, f.kept.size());
    f.kept[0].resolve("unheard");
    f.kept.clear();

    EXPECT_TRUE(f.sent.empty());
}

TEST(dispatcher, gateHoldsLaterRequestsUntilReleased)
{
    DispatcherFixture f;

    f.dispatcher->dispatch(request(1, "gated"));
    f.dispatcher->dispatch(request(2, "fast"));
    f.dispatcher->dispatch(request(3, "slow"));

    EXPECT_TRUE(f.sent.empty());
    EXPECT_EQ((std::vector<std::string>{ "gated" }), f.effects);

    // The effect is done but the reply is not: the requests behind it run and
    // their replies overtake it.
    f.gates.clear();

    EXPECT_EQ((std::vector<std::string>{ "gated", "fast", "slow" }), f.effects);
    EXPECT_EQ((std::vector<int>{ 2 }), f.sentIds());

    f.kept[0].resolve("gated");
    f.kept[1].resolve("slow");

    EXPECT_EQ((std::vector<int>{ 2, 1, 3 }), f.sentIds());
}

TEST(dispatcher, gatedRequestTakingTheGateAgainHoldsTheRest)
{
    DispatcherFixture f;

    f.dispatcher->dispatch(request(1, "gated"));
    f.dispatcher->dispatch(request(2, "gated"));
    f.dispatcher->dispatch(request(3, "fast"));

    ASSERT_EQ(1u, f.gates.size());
    f.gates[0].release();

    EXPECT_EQ((std::vector<std::string>{ "gated", "gated" }), f.effects);
    ASSERT_EQ(2u, f.gates.size());

    f.gates[1].release();

    EXPECT_EQ((std::vector<std::string>{ "gated", "gated", "fast" }),
            f.effects);
    EXPECT_EQ((std::vector<int>{ 3 }), f.sentIds());

    f.kept.clear();
}

TEST(dispatcher, answerAfterTheSessionEndedGoesNowhere)
{
    DispatcherFixture f;

    f.dispatcher->dispatch(request(1, "slow"));
    f.dispatcher->dispatch(request(2, "slow"));
    f.dispatcher->dispatch(request(3, "gated"));
    f.dispatcher->dispatch(request(4, "fast"));

    f.dispatcher->end();

    f.kept[0].resolve("late");
    f.gates.clear();

    EXPECT_TRUE(f.sent.empty());
    EXPECT_EQ((std::vector<std::string>{ "slow", "slow", "gated" }),
            f.effects);

    // The responders and the gate outlive the dispatcher itself.
    DispatchGate gate = f.dispatcher->holdGate();
    f.dispatcher.reset();

    f.kept[1].reject({ kTestError, "later still" });
    gate.release();
    f.kept.clear();

    EXPECT_TRUE(f.sent.empty());
}

TEST(dispatcher, stopDiscardsHeldRequestsButLetsRepliesOut)
{
    DispatcherFixture f;

    f.dispatcher->dispatch(request(1, "gated"));
    f.dispatcher->dispatch(request(2, "fast"));

    f.dispatcher->stop();
    f.gates.clear();
    f.dispatcher->dispatch(request(3, "fast"));

    EXPECT_EQ((std::vector<std::string>{ "gated" }), f.effects);

    f.kept[0].resolve("done");

    EXPECT_EQ((std::vector<int>{ 1 }), f.sentIds());
}

TEST(dispatcher, handlerMayDestroyTheDispatcher)
{
    DispatcherFixture f;
    f.dispatcher->add({ "end", "", {},
        [&f](json const&, Responder&)
        {
            f.dispatcher->dispatch(request(2, "fast"));
            f.dispatcher.reset();
            return json::object();
        } });

    f.dispatcher->dispatch(request(1, "end"));

    EXPECT_FALSE(f.dispatcher);
    EXPECT_TRUE(f.sent.empty());
    EXPECT_TRUE(f.effects.empty());
}
