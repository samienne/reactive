#include "dispatcher.h"

#include <deque>
#include <exception>
#include <utility>

namespace bqui::remote
{

using nlohmann::json;

struct DispatchState
{
    std::function<void(std::string const&)> send;
    std::vector<Method> methods;
    std::deque<std::string> held;
    int gates = 0;
    bool pumping = false;
    bool stopped = false;
    bool ended = false;
};

struct PendingRequest
{
    std::weak_ptr<DispatchState> state;
    json id;
    bool notification = false;
    bool answered = false;
};

namespace
{

std::string resultFrame(json const& id, json const& result)
{
    json reply = {
        { "jsonrpc", "2.0" },
        { "id", id },
        { "result", result },
    };
    return reply.dump();
}

std::string errorFrame(json const& id, int code, std::string const& message,
        json const& data = json())
{
    json error = {
        { "code", code },
        { "message", message },
    };
    if (!data.is_null())
        error["data"] = data;

    json reply = {
        { "jsonrpc", "2.0" },
        { "id", id },
        { "error", error },
    };
    return reply.dump();
}

// Marks the request answered and returns where the answer goes, if anywhere.
// A request without an `id` is a notification: never answered, even on error.
std::shared_ptr<DispatchState> claim(PendingRequest& request)
{
    if (request.answered)
        return nullptr;

    request.answered = true;

    auto state = request.state.lock();
    if (!state || state->ended || request.notification)
        return nullptr;

    return state;
}

void answerError(PendingRequest& request, RpcError const& error)
{
    if (auto state = claim(request))
        state->send(errorFrame(request.id, error.code, error.message,
                    error.data));
}

void answerResult(PendingRequest& request, json const& result)
{
    auto state = claim(request);
    if (!state)
        return;

    std::string frame;
    try
    {
        frame = resultFrame(request.id, result);
    }
    catch (std::exception const& e)
    {
        frame = errorFrame(request.id, kInternalError, e.what());
    }

    state->send(frame);
}

void checkRequiredParams(Method const& method, json const& params)
{
    for (auto const& param : method.params)
    {
        if (!param.required)
            continue;

        if (!params.is_object() || !params.contains(param.name))
            throw RpcError{ kInvalidParams,
                std::string("missing required parameter '")
                    + param.name + "'" };
    }
}

Method const* findMethod(DispatchState const& state, std::string const& name)
{
    for (auto const& method : state.methods)
        if (method.name == name)
            return &method;

    return nullptr;
}

} // namespace

Responder::Responder() = default;

Responder::Responder(std::shared_ptr<PendingRequest> request) :
    request_(std::move(request))
{
}

Responder::Responder(Responder&& other) noexcept :
    request_(std::move(other.request_))
{
    other.request_ = nullptr;
}

Responder& Responder::operator=(Responder&& other) noexcept
{
    if (this != &other)
    {
        drop();
        request_ = std::move(other.request_);
        other.request_ = nullptr;
    }

    return *this;
}

Responder::~Responder()
{
    drop();
}

void Responder::resolve(json result)
{
    if (request_)
        answerResult(*request_, result);
}

void Responder::reject(RpcError error)
{
    if (request_)
        answerError(*request_, error);
}

void Responder::drop()
{
    if (!request_)
        return;

    try
    {
        answerError(*request_, { kInternalError,
                "the request was dropped without an answer" });
    }
    catch (...)
    {
    }

    request_ = nullptr;
}

DispatchGate::DispatchGate() = default;

DispatchGate::DispatchGate(std::weak_ptr<DispatchState> state) :
    state_(std::move(state))
{
}

DispatchGate::DispatchGate(DispatchGate&& other) noexcept :
    state_(std::move(other.state_))
{
    other.state_.reset();
}

DispatchGate& DispatchGate::operator=(DispatchGate&& other) noexcept
{
    if (this != &other)
    {
        release();
        state_ = std::move(other.state_);
        other.state_.reset();
    }

    return *this;
}

DispatchGate::~DispatchGate()
{
    release();
}

void DispatchGate::release()
{
    auto state = state_.lock();
    state_.reset();

    if (state && --state->gates == 0)
        Dispatcher::pump(state);
}

Dispatcher::Dispatcher(std::function<void(std::string const&)> send) :
    state_(std::make_shared<DispatchState>())
{
    state_->send = std::move(send);
}

Dispatcher::~Dispatcher()
{
    end();
}

void Dispatcher::add(Method method)
{
    state_->methods.push_back(std::move(method));
}

std::vector<Method> const& Dispatcher::methods() const
{
    return state_->methods;
}

void Dispatcher::dispatch(std::string frame)
{
    if (state_->stopped)
        return;

    state_->held.push_back(std::move(frame));
    pump(state_);
}

DispatchGate Dispatcher::holdGate()
{
    ++state_->gates;
    return DispatchGate(state_);
}

void Dispatcher::stop()
{
    state_->stopped = true;
    state_->held.clear();
}

void Dispatcher::end()
{
    stop();
    state_->ended = true;
}

void Dispatcher::pump(std::shared_ptr<DispatchState> const& state)
{
    if (state->pumping)
        return;

    struct Pumping
    {
        bool& flag;
        explicit Pumping(bool& f) : flag(f) { flag = true; }
        ~Pumping() { flag = false; }
    } pumping(state->pumping);

    while (!state->held.empty() && state->gates == 0 && !state->stopped)
    {
        std::string frame = std::move(state->held.front());
        state->held.pop_front();
        handle(state, frame);
    }
}

void Dispatcher::handle(std::shared_ptr<DispatchState> const& state,
        std::string const& frame)
{
    json message = json::parse(frame, nullptr, /*allow_exceptions=*/false);

    if (message.is_discarded())
    {
        state->send(errorFrame(json(), kParseError, "Parse error"));
        return;
    }

    if (!message.is_object())
    {
        state->send(errorFrame(json(), kInvalidRequest, "Invalid request"));
        return;
    }

    json id = message.contains("id") ? message["id"] : json();

    auto methodIt = message.find("method");
    if (methodIt == message.end() || !methodIt->is_string())
    {
        state->send(errorFrame(id, kInvalidRequest,
                    "Invalid request: 'method' must be a string"));
        return;
    }

    auto request = std::make_shared<PendingRequest>();
    request->state = state;
    request->id = std::move(id);
    request->notification = !message.contains("id");

    std::string name = methodIt->get<std::string>();
    json params = message.contains("params") ? message["params"]
                                              : json::object();

    Method const* method = findMethod(*state, name);
    if (!method)
    {
        answerError(*request, { kMethodNotFound,
                "Method not found: " + name });
        return;
    }

    // Neither answered nor kept by the handler, the responder answers with an
    // internal error as it goes out of scope.
    Responder responder(request);

    try
    {
        checkRequiredParams(*method, params);
        if (auto result = method->handler(params, responder))
            answerResult(*request, *result);
    }
    catch (RpcError const& e)
    {
        answerError(*request, e);
    }
    catch (std::exception const& e)
    {
        answerError(*request, { kInternalError, e.what() });
    }
}

} // namespace bqui::remote
