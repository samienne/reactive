# bqui MCP bridge

`bqui_mcp.py` bridges the bqui **inspector protocol** (JSON-RPC 2.0 over a
framed stream, see [`docs/design/inspector-protocol.md`](../../docs/design/inspector-protocol.md))
to **MCP**, so an MCP client such as Claude Code can drive a headless bqui app
as a set of tools: list windows, read a window's widget tree, inject input, and
step frames.

The app is client-agnostic — it speaks plain JSON-RPC and knows nothing about
MCP. This bridge owns the socket (framing, request-id correlation, app
notifications) and generates one MCP tool per protocol method from the app's own
`system.describe`, so the tool list never drifts from the protocol.

## How it runs

The app *connects* to an endpoint the client *listens* on. The bridge therefore
opens a loopback TCP listener, launches `inspectorapp` pointed at it, accepts the
connection, calls `system.describe`, and serves MCP over stdio.

## Requirements

- Python 3.10+ and the `mcp` package: `pip install -r tools/mcp/requirements.txt`
- A built tree containing `inspectorapp` (Meson/Ninja — see the top-level
  `AGENTS.md`).

## Configuration (environment)

| Var | Meaning |
|---|---|
| `BQUI_BUILD_DIR` | Meson build dir. Its `*.dll` directories are put on `PATH` so the app's sibling DLLs resolve. Also locates `inspectorapp` by default. |
| `BQUI_INSPECTORAPP` | Explicit path to the app exe (overrides the default under `BQUI_BUILD_DIR`). |
| `BQUI_LAUNCH` | Full launch command template overriding the above; `{endpoint}` is substituted. |

## Smoke test (no MCP client)

```sh
BQUI_BUILD_DIR=/path/to/build python tools/mcp/bqui_mcp.py --selftest
```
Launches the app, prints the generated tools, queries `window.list` /
`window.introspect`, and exits.

## Register with Claude Code

```sh
claude mcp add bqui -e BQUI_BUILD_DIR=/path/to/build -- python /abs/path/to/tools/mcp/bqui_mcp.py
```

Then, in a **fresh** Claude Code session, the tools appear as `mcp__bqui__*`
(`window_list`, `window_introspect`, `window_inject`, `app_step`, ...).
