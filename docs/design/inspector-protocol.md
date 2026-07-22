# The inspector protocol

*Design record. The implementation lands in stages; this describes the whole
shape. Supersedes the interim agent protocol first sketched in PR #96/#123.*

## What it is, and who talks to it

A running bqui app exposes a JSON-RPC control channel. An **out-of-process
client** launches the app, drives its execution (run / pause / step), and
queries its state (which windows exist, each window's widget tree and render
tree). Two clients are expected, and they are the *same* protocol:

- an **agent** — an autonomous driver that pauses, inspects, injects input, and
  advances frames deterministically;
- an **editor / inspector** — a human-facing tool that hosts the app, lets a
  developer freeze it and look inside, and later *augment* what it draws.

The agent is not a special case; it is the inspector protocol used by a program
instead of a person. Designing for the inspector gives us the agent for free
and keeps the channel honest: everything an agent needs is something a debugger
would also want.

## Envelope: JSON-RPC 2.0

One frame on the transport carries one JSON-RPC 2.0 message. No new spec of our
own.

- **Request** `{"jsonrpc":"2.0","id":<n>,"method":<s>,"params":{…}}` — always
  answered, with the same `id`.
- **Response** `{"jsonrpc":"2.0","id":<n>,"result":{…}}` or
  `{"jsonrpc":"2.0","id":<n>,"error":{"code":<n>,"message":<s>,"data":{…}}}`.
- **Notification** `{"jsonrpc":"2.0","method":<s>,"params":{…}}` — no `id`, no
  response. Flows **either** direction: app→client for logs and frame ticks;
  client→app for fire-and-forget settings.

Requests carry an `id` and may be answered out of order — the channel is fully
asynchronous. The `id` is the client's; the app only echoes it. Batching (an
array of messages) is not implemented until something needs it.

## Concurrency: a reader thread, a single-threaded app

The app thread stays single-threaded — it owns every window, its signals, and
`withAnimation`, exactly as today. To let a `pause` or a query arrive *while the
app is free-running*, a dedicated **transport reader thread** decodes frames
into a thread-safe **command queue**. The app thread is the only one that
touches windows, sends responses, and emits notifications; the reader thread
only enqueues. The queue (mutex + condition variable) is the one shared object,
and it is the synchronization boundary.

Per app tick the app thread drains the queue at a coherent point — between
`signalContext.update` frames — so a query is always answered against a whole,
untorn frame. When **paused**, the app thread blocks on the queue instead of
spinning. The transport must allow a concurrent send (app thread) and receive
(reader thread); the pipe and socket transports are full-duplex, so this holds.

(The reader thread is only needed to interrupt a free run. The paused/step
subset — block on the queue, answer, step on request — needs no second thread,
and is implemented first.)

## Execution model: launch paused; run / pause / step

The app builds its windows and then **stops before frame 1, paused**, and waits
for commands. Nothing advances until asked.

- `app.run` — enter RUNNING: advance frames (real-time under a windowed
  platform, as fast as stepped under the headless one) until a `pause` arrives
  or the app exits. Frame notifications are emitted only if subscribed.
- `app.pause` — enter PAUSED: stop advancing; hold the current frame.
- `app.step {count?, dt_us?}` — advance `count` frames (default 1) by `dt_us`
  each (default one nominal frame), then PAUSED. This is the agent's
  deterministic clock.
- `app.shutdown` — end the run and let the process exit.

**Queries do not force a pause.** Because the command queue is drained at a
coherent point every frame, `window.introspect` / `window.renderTree` return an
untorn frame whether the app is running or paused. A client that wants a frozen
target (an agent about to act, a developer reading a value) sends `pause`
first; a client that wants a live peek queries while running. Auto-pause is a
client policy, not a protocol rule.

## Methods

The set is a **registry**: each method carries a name, a one-line description,
and a parameter schema. `system.describe` returns the whole registry, so a
client can discover what the app supports and what each call needs without
hard-coding it. New capability = new registry entry. The registry metadata is a
first-class part of the implementation, not an afterthought — it is the contract
the agent's toolset is generated from (see below).

Initial methods:

| Method | Params | Result |
|---|---|---|
| `system.describe` | — | `{methods:[{name, doc, params:[{name, type, required}]}]}` |
| `app.run` | — | `{state:"running"}` |
| `app.pause` | — | `{state:"paused", frame:<n>}` |
| `app.step` | `{count?, dt_us?}` | `{state:"paused", frame:<n>}` |
| `app.shutdown` | — | `{}` |
| `app.setFrameNotifications` | `{enabled}` | `{}` |
| `window.list` | — | `{windows:[{id}]}` (basic info only) |
| `window.introspect` | `{window:<id>}` | `{introspection:{…}}` (widget tree) |
| `window.renderTree` | `{window:<id>}` | `{renderTree:{…}}` (avg snapshot) |
| `window.inject` | `{window:<id>, events:[…]}` | `{}` |

`window.list` returns just identity (with room for a little basic info later —
size, focus). The heavy views are separate per-window queries, each taking a
window `id`, so a client pays for a tree only when it asks for that window.

`window.renderTree` depends on avg's snapshot, so it lands one layer up the
stack (with #122) rather than in the core.

## Reaching the protocol from an LLM agent

The app is client-agnostic and stays that way: it speaks one JSON-RPC surface
and knows nothing about who is on the other end. A human editor speaks it
directly. An **LLM agent reaches it through a separate bridge**, never by
writing to the pipe itself — the transport is length-prefixed binary frames an
LLM cannot emit, and the async id/notification bookkeeping does not belong in a
model's context.

That bridge exposes each protocol method as a typed **tool**, generated from
`system.describe`, and owns the socket: framing, id/response correlation,
notification demux, and result shaping (filtering a large tree before it reaches
the model). It is naturally an **MCP server** — MCP is itself JSON-RPC 2.0, so
the bridge is close to a dialect translation, and any MCP-capable agent can then
drive a bqui app as a set of tools. The bridge lives outside the app as its own
component; nothing about it leaks into bqui.

## Notifications

The app is silent by default. The only things it may volunteer:

- `log` `{level:"error"|"warning"|"info", message}` — always allowed;
  diagnostics the client should see.
- `frame` `{index, dt_us, …timings}` — **opt-in**, toggled by
  `app.setFrameNotifications`; off by default. A per-frame heartbeat for an
  editor that wants to graph timings.

## Windows: identified by id alone

A window is identified by its `id` — the `btl::UniqueId` it already carries,
serialized as its `uint64` value. **No title on the wire.** A title is content,
it can change mid-run, and it is available inside `window.introspect`; using it
as identity would be a bug waiting to happen. This drops the `title` key the
interim protocol put on window identity.

## JSON handling

nlohmann/json (from wrapdb, a Meson subproject) owns the envelope, request
parsing, and response/notification building — confined to the bqui agent layer,
with `json.hpp` included only in `.cpp` files so the compile cost does not
spread. The hand-rolled `parseJson` and the introspection string-builder are
retired in its favour.

`avg::toJson(avg::Snapshot)` stays — the recorded decision to keep avg's
snapshot JSON hand-written and avg dependency-free stands. The agent layer
embeds its output as a sub-document (parse-and-insert) under
`window.renderTree`'s `renderTree`, so there is one snapshot serializer and avg
gains no dependency.

## What carries over from the interim protocol

The execution seams already built survive unchanged in substance:

- the dynamic-window reconcile — the `AgentApp` seam (`reconcile(dt)` +
  `liveWindows()`) driving the fused `signalContext` so the window set is live;
- `WindowGlue::snapshot()` (render tree) and `getResolvedIntrospection()`
  (widget tree), and the per-window `inject*` seam.

What changes: the transport *payload* (bespoke commands → JSON-RPC), flow
control (session-drives-every-frame → launch-paused + run/pause/step over the
command queue), window identity (id+title → id), and the JSON layer. The rework
lands in the agentic-platform branch; the `renderTree` method and its avg
dependency stay one layer up with #122/#123.

## Future, explicitly out of scope now

- **Augmenting the render tree over the API** — a method that injects nodes so a
  client can draw onto a window (inspector overlays, agent annotations). The
  registry and the embed-avg-JSON seam leave room; not built now.
- Batching; multiple simultaneous clients; authentication.

## Non-goals

The protocol does not expose the signal graph, and does not let a client mutate
widget state directly — control is through input injection and flow, and
observation is through the two trees. That keeps the app the single owner of its
own state.
