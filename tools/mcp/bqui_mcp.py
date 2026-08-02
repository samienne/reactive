#!/usr/bin/env python3
"""MCP bridge for the bqui inspector protocol.

Exposes a running bqui app's JSON-RPC "inspector protocol" as MCP tools, so an
MCP client (Claude Code, Claude Desktop, ...) can drive a headless bqui app:
list its windows, read each window's widget/render tree, inject input, and step
frames.

The app is client-agnostic and speaks plain JSON-RPC 2.0 over a framed stream.
This bridge owns that stream (framing, request-id correlation, notifications)
and re-exposes every protocol method as an MCP tool. The tool list is generated
at startup from the app's own `system.describe`, so it never drifts.

Lifecycle: the app CONNECTS to an endpoint the client LISTENS on. So this
bridge opens a loopback TCP listener, launches inspectorapp pointed at it,
accepts the connection, calls `system.describe`, then serves MCP over stdio.

Configuration (environment):
  BQUI_BUILD_DIR     Meson build dir. Its *.dll directories are put on PATH so
                     the app's sibling DLLs resolve. Required unless BQUI_LAUNCH.
  BQUI_INSPECTORAPP  Path to the app exe (default: <build>/src/inspectorapp/
                     inspectorapp.exe, or inspectorapp on POSIX).
  BQUI_LAUNCH        Full launch command template overriding the above;
                     "{endpoint}" is substituted. PATH is still augmented from
                     BQUI_BUILD_DIR if set.

Run `python bqui_mcp.py --selftest` to launch the app, describe it, print the
generated tools, and exit — no MCP client needed.
"""
from __future__ import annotations

import asyncio
import glob
import json
import os
import shlex
import struct
import sys
from typing import Any

LENGTH_PREFIX = 4  # bytes, big-endian; matches the StreamTransport framing.


def log(*a: Any) -> None:
    print("[bqui-mcp]", *a, file=sys.stderr, flush=True)


def app_env() -> dict:
    env = dict(os.environ)
    build = env.get("BQUI_BUILD_DIR")
    if build:
        dll_dirs = sorted({os.path.dirname(p)
                           for p in glob.glob(os.path.join(build, "**", "*.dll"),
                                              recursive=True)})
        env["PATH"] = os.pathsep.join(dll_dirs + [env.get("PATH", "")])
    return env


def app_command(endpoint: str) -> list[str]:
    template = os.environ.get("BQUI_LAUNCH")
    if template:
        return shlex.split(template.replace("{endpoint}", endpoint))
    exe = os.environ.get("BQUI_INSPECTORAPP")
    if not exe:
        build = os.environ.get("BQUI_BUILD_DIR")
        if not build:
            raise RuntimeError("set BQUI_BUILD_DIR (or BQUI_INSPECTORAPP / BQUI_LAUNCH)")
        exe = os.path.join(build, "src", "inspectorapp",
                           "inspectorapp.exe" if os.name == "nt" else "inspectorapp")
    return [exe, endpoint]


class AppConnection:
    """Owns the framed JSON-RPC stream to one bqui app process."""

    def __init__(self) -> None:
        self._reader = None
        self._writer = None
        self._next_id = 1
        self._pending: dict[int, asyncio.Future] = {}
        self._proc = None
        self._server = None
        self._connected: asyncio.Future | None = None

    async def start(self) -> None:
        self._connected = asyncio.get_event_loop().create_future()
        # Keep the listener open for the connection's lifetime — closing it
        # right after accept resets the accepted socket on Windows Proactor.
        self._server = await asyncio.start_server(self._on_connected, "127.0.0.1", 0)
        port = self._server.sockets[0].getsockname()[1]
        endpoint = f"tcp://127.0.0.1:{port}"
        log(f"listening on {endpoint}")

        cmd = app_command(endpoint)
        log("launching:", " ".join(cmd))
        self._proc = await asyncio.create_subprocess_exec(
            *cmd, stdout=asyncio.subprocess.DEVNULL,
            stderr=asyncio.subprocess.DEVNULL, env=app_env())

        try:
            await asyncio.wait_for(self._connected, timeout=30)
        except asyncio.TimeoutError:
            raise RuntimeError("bqui app did not connect within 30s")

    async def _on_connected(self, reader, writer) -> None:
        if self._reader is not None:
            writer.close()
            return
        self._reader, self._writer = reader, writer
        asyncio.create_task(self._read_loop())
        log("app connected")
        if not self._connected.done():
            self._connected.set_result(True)

    async def _read_loop(self) -> None:
        try:
            while True:
                header = await self._reader.readexactly(LENGTH_PREFIX)
                (length,) = struct.unpack(">I", header)
                payload = await self._reader.readexactly(length)
                self._on_message(json.loads(payload.decode("utf-8")))
        except (asyncio.IncompleteReadError, ConnectionError):
            log("app disconnected")
            for fut in self._pending.values():
                if not fut.done():
                    fut.set_exception(ConnectionError("app disconnected"))

    def _on_message(self, msg: dict) -> None:
        if msg.get("id") is not None and ("result" in msg or "error" in msg):
            fut = self._pending.pop(msg["id"], None)
            if fut and not fut.done():
                fut.set_result(msg)
        elif "method" in msg:  # a notification from the app
            p = msg.get("params", {})
            if msg["method"] == "log":
                log(f"app-{p.get('level','info')}: {p.get('message','')}")
            else:
                log(f"notification {msg['method']}: {p}")

    async def request(self, method: str, params: dict | None = None) -> Any:
        rid = self._next_id
        self._next_id += 1
        frame = {"jsonrpc": "2.0", "id": rid, "method": method}
        if params:
            frame["params"] = params
        fut = asyncio.get_event_loop().create_future()
        self._pending[rid] = fut
        data = json.dumps(frame).encode("utf-8")
        self._writer.write(struct.pack(">I", len(data)) + data)
        await self._writer.drain()
        resp = await fut
        if "error" in resp:
            e = resp["error"]
            raise RuntimeError(f"{method} failed [{e.get('code')}]: {e.get('message')}")
        return resp.get("result")

    async def close(self) -> None:
        try:
            if self._writer:
                await self.request("app.shutdown")
        except Exception:
            pass
        if self._proc and self._proc.returncode is None:
            try:
                self._proc.terminate()
            except ProcessLookupError:
                pass
        if self._server:
            self._server.close()


_TYPE_MAP = {"int": "integer", "uint": "integer", "number": "number",
             "bool": "boolean", "string": "string", "array": "array",
             "object": "object"}


def tool_schema(method: dict) -> dict:
    props, required = {}, []
    for p in method.get("params", []):
        props[p["name"]] = {"type": _TYPE_MAP.get(p.get("type", "string"), "string")}
        if p.get("required"):
            required.append(p["name"])
    return {"type": "object", "properties": props, "required": required}


async def run_mcp(conn: AppConnection, methods: list[dict]) -> None:
    import mcp.server.stdio
    import mcp.types as types
    from mcp.server import Server

    tools = [types.Tool(name=m["name"].replace(".", "_"),
                        description=m.get("doc", ""),
                        inputSchema=tool_schema(m)) for m in methods]
    by_tool = {t.name: m["name"] for t, m in zip(tools, methods)}
    log(f"exposing {len(tools)} tools:", ", ".join(t.name for t in tools))

    server = Server("bqui")

    @server.list_tools()
    async def list_tools():
        return tools

    @server.call_tool()
    async def call_tool(name: str, arguments: dict):
        method = by_tool.get(name)
        if method is None:
            raise ValueError(f"unknown tool {name}")
        result = await conn.request(method, arguments or None)
        return [types.TextContent(type="text", text=json.dumps(result, indent=2))]

    async with mcp.server.stdio.stdio_server() as (r, w):
        await server.run(r, w, server.create_initialization_options())


async def selftest(conn: AppConnection, methods: list[dict]) -> int:
    print("methods:", [m["name"] for m in methods])
    for m in methods:
        print(f"  tool {m['name'].replace('.', '_')}  params={[p['name'] for p in m.get('params', [])]}")
    wl = await conn.request("window.list")
    print("window.list:", wl)
    if wl.get("windows"):
        wid = wl["windows"][0]["id"]
        intro = await conn.request("window.introspect", {"window": wid})
        print("introspect ok, root type:", intro["introspection"].get("role") or intro["introspection"].get("name"))
    print("SELFTEST OK")
    return 0


async def amain() -> int:
    conn = AppConnection()
    await conn.start()
    described = await conn.request("system.describe")
    methods = described.get("methods", [])
    try:
        if "--selftest" in sys.argv:
            return await selftest(conn, methods)
        await run_mcp(conn, methods)
        return 0
    finally:
        await conn.close()


def main() -> None:
    try:
        sys.exit(asyncio.run(amain()))
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
