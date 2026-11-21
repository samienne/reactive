#pragma once

#include "bqui/bquivisibility.h"

#include <nlohmann/json.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace bqui::remote
{
    // Standard JSON-RPC 2.0 error codes.
    inline constexpr int kParseError = -32700;
    inline constexpr int kInvalidRequest = -32600;
    inline constexpr int kMethodNotFound = -32601;
    inline constexpr int kInvalidParams = -32602;
    inline constexpr int kInternalError = -32603;

    /**
     * @brief A handler failure carrying a JSON-RPC error code.
     *
     * A handler throws one, or passes one to `Responder::reject`.
     */
    struct BQUI_EXPORT RpcError
    {
        int code;
        std::string message;
        nlohmann::json data = nullptr;
    };

    struct ParamSpec
    {
        char const* name;
        char const* type;
        bool required;
    };

    struct DispatchState;
    struct PendingRequest;

    /**
     * @brief Answers one request, now or later.
     *
     * A handler that cannot answer before it returns moves the responder out
     * of its argument, keeps it, and calls `resolve` or `reject` once the
     * answer exists. Only the first answer to a request counts, and one given
     * after the session ended goes nowhere. Destroying a kept responder that
     * never answered sends an internal error, so the client is not left
     * waiting. Use it on the loop thread only: work that finishes on another
     * thread posts back to the loop first.
     */
    class BQUI_EXPORT Responder
    {
    public:
        Responder();
        Responder(Responder&& other) noexcept;
        Responder& operator=(Responder&& other) noexcept;
        Responder(Responder const&) = delete;
        Responder& operator=(Responder const&) = delete;
        ~Responder();

        void resolve(nlohmann::json result);
        void reject(RpcError error);

    private:
        friend class Dispatcher;
        explicit Responder(std::shared_ptr<PendingRequest> request);

        void drop();

        std::shared_ptr<PendingRequest> request_;
    };

    /**
     * @brief Holds back the requests behind a deferred one.
     *
     * While any gate is held, requests that arrive are kept in order and not
     * dispatched. Releasing the last gate, or destroying it, dispatches them.
     * A handler holds one for as long as its effect on the app is unfinished,
     * which may be shorter than the wait for its reply. Loop thread only.
     */
    class BQUI_EXPORT DispatchGate
    {
    public:
        DispatchGate();
        DispatchGate(DispatchGate&& other) noexcept;
        DispatchGate& operator=(DispatchGate&& other) noexcept;
        DispatchGate(DispatchGate const&) = delete;
        DispatchGate& operator=(DispatchGate const&) = delete;
        ~DispatchGate();

        void release();

    private:
        friend class Dispatcher;
        explicit DispatchGate(std::weak_ptr<DispatchState> state);

        std::weak_ptr<DispatchState> state_;
    };

    /**
     * @brief What a handler returns: its result, or `pending` when it kept the
     * responder to answer later.
     */
    using Reply = std::optional<nlohmann::json>;

    inline constexpr std::nullopt_t pending = std::nullopt;

    /**
     * @brief One remote method.
     *
     * The usual handler ignores the responder and returns its result:
     * `[](json const& params, Responder&) { return json{ { "ok", true } }; }`.
     * It reports a failure by throwing an `RpcError`.
     */
    struct Method
    {
        std::string name;
        std::string doc;
        std::vector<ParamSpec> params;
        std::function<Reply(nlohmann::json const& params,
                Responder& responder)> handler;
    };

    /**
     * @brief The JSON-RPC side of a remote session: parses request frames,
     * calls the registered methods and hands reply frames to `send`.
     *
     * Requests are dispatched in the order they arrive, one handler returning
     * before the next is called. Replies need not come in that order: a
     * deferred one may be overtaken, and the `id` tells them apart. Loop
     * thread only.
     */
    class BQUI_EXPORT Dispatcher
    {
    public:
        explicit Dispatcher(std::function<void(std::string const&)> send);
        Dispatcher(Dispatcher const&) = delete;
        Dispatcher& operator=(Dispatcher const&) = delete;
        ~Dispatcher();

        void add(Method method);
        std::vector<Method> const& methods() const;

        void dispatch(std::string frame);

        DispatchGate holdGate();

        /**
         * @brief Dispatch nothing more and discard the requests held back by a
         * gate. Replies still go out.
         */
        void stop();

        /**
         * @brief End the session: `stop`, and every responder still out
         * answers nowhere from now on.
         */
        void end();

    private:
        friend class DispatchGate;

        static void pump(std::shared_ptr<DispatchState> const& state);
        static void handle(std::shared_ptr<DispatchState> const& state,
                std::string const& frame);

        std::shared_ptr<DispatchState> state_;
    };
} // namespace bqui::remote
