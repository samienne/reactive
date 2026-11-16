#!/usr/bin/env python3
"""MCP bridge for the bqui inspector protocol.

Exposes a running bqui app's JSON-RPC "inspector protocol" as MCP tools, so an
MCP client (Claude Code, Claude Desktop, ...) can drive a headless bqui app:
list its windows, read each window's widget/render tree, inject input, and step
frames.

The app is client-agnostic and speaks plain JSON-RPC 2.0 over a framed stream.
The bqui_remote module next to this file owns that stream; this bridge
re-exposes every protocol method as an MCP tool. The tool list is generated
at startup from the app's own `system.describe`, so it never drifts. The
`blob.*` methods are not tools: when a method answers with a blob descriptor,
the bridge pulls the blob, releases it, and returns its content instead.

Lifecycle: the app CONNECTS to an endpoint the client LISTENS on. So this
bridge opens a loopback TCP listener, launches inspectorapp pointed at it,
accepts the connection, calls `system.describe`, then serves MCP over stdio.

Configuration: the BQUI_BUILD_DIR, BQUI_INSPECTORAPP and BQUI_LAUNCH
environment variables that bqui_remote describes.

Run `python bqui_mcp.py --selftest` to launch the app, describe it, print the
generated tools, and exit — no MCP client needed. `--save-screenshot <path>`
also writes the self-test's screenshot there.
"""
from __future__ import annotations

import argparse
import asyncio
import base64
import json
import logging
import struct
import sys
import zlib
from itertools import accumulate
from typing import Any

from bqui_remote import Connection, blob_read_length, read_blob


def log(*a: Any) -> None:
    print("[bqui-mcp]", *a, file=sys.stderr, flush=True)


def log_notification(method: str, params: Any) -> None:
    if method == "log":
        log(f"app-{params.get('level','info')}: {params.get('message','')}")
    else:
        log(f"notification {method}: {params}")


def log_to_stderr() -> None:
    handler = logging.StreamHandler(sys.stderr)
    handler.setFormatter(logging.Formatter("[bqui-mcp] %(message)s"))
    remote = logging.getLogger("bqui_remote")
    remote.addHandler(handler)
    remote.setLevel(logging.INFO)
    remote.propagate = False


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


def tool_methods(methods: list[dict]) -> list[dict]:
    return [m for m in methods if not m["name"].startswith("blob.")]


def blob_content(types: Any, blob: dict, data: bytes) -> Any:
    content_type = blob.get("contentType", "")
    if content_type.startswith("image/"):
        return types.ImageContent(type="image", mimeType=content_type,
                                  data=base64.b64encode(data).decode("ascii"))
    if content_type.startswith("text/") or content_type.endswith("json"):
        return types.TextContent(type="text", text=data.decode("utf-8"))
    return types.TextContent(
        type="text", text=f"{len(data)} bytes of {content_type}, base64: "
                          + base64.b64encode(data).decode("ascii"))


async def run_mcp(conn: Connection, methods: list[dict],
                  read_length: int | None) -> None:
    import mcp.server.stdio
    import mcp.types as types
    from mcp.server import Server

    methods = tool_methods(methods)
    returns_blob = {m["name"] for m in methods if m.get("returnsBlob")}
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
        blob = result.get("blob") if isinstance(result, dict) else None
        if method in returns_blob and isinstance(blob, dict):
            data = await read_blob(conn, blob, read_length)
            content = [blob_content(types, blob, data)]
            metadata = {k: v for k, v in result.items() if k != "blob"}
            if metadata:
                content.append(types.TextContent(
                    type="text", text=json.dumps(metadata, indent=2)))
            return content
        return [types.TextContent(type="text", text=json.dumps(result, indent=2))]

    async with mcp.server.stdio.stdio_server() as (r, w):
        await server.run(r, w, server.create_initialization_options())


def png_pixels(data: bytes) -> tuple[int, int, list[bytes]]:
    """Decodes an 8-bit RGB or RGBA PNG to its size and unfiltered rows."""
    if not data.startswith(b"\x89PNG\r\n\x1a\n"):
        raise RuntimeError("not a PNG")
    pos, ihdr, idat = 8, None, b""
    while pos < len(data):
        (length,) = struct.unpack(">I", data[pos:pos + 4])
        kind = data[pos + 4:pos + 8]
        body = data[pos + 8:pos + 8 + length]
        (crc,) = struct.unpack(">I", data[pos + 8 + length:pos + 12 + length])
        if zlib.crc32(kind + body) != crc:
            raise RuntimeError(f"PNG chunk {kind!r}: bad CRC")
        if kind == b"IHDR":
            ihdr = struct.unpack(">IIBBBBB", body)
        elif kind == b"IDAT":
            idat += body
        elif kind == b"IEND":
            break
        pos += 12 + length
    if ihdr is None:
        raise RuntimeError("PNG has no IHDR")
    width, height, depth, colour, _, _, interlace = ihdr
    if not width or not height:
        raise RuntimeError(f"PNG is {width}x{height}")
    channels = {2: 3, 6: 4}.get(colour)
    if depth != 8 or channels is None or interlace:
        raise RuntimeError(f"PNG format not handled: depth {depth}, "
                           f"colour type {colour}, interlace {interlace}")
    raw = zlib.decompress(idat)
    stride = width * channels
    if len(raw) != height * (stride + 1):
        raise RuntimeError("PNG image data has the wrong size")
    low = int.from_bytes(b"\x7f" * stride, "big")
    high = int.from_bytes(b"\x80" * stride, "big")
    rows, prior = [], bytearray(stride)
    for y in range(height):
        start = y * (stride + 1)
        kind, line = raw[start], bytearray(raw[start + 1:start + 1 + stride])
        if kind == 1:
            for ch in range(channels):
                line[ch::channels] = bytes(v & 0xFF for v in
                                           accumulate(line[ch::channels]))
        elif kind == 2:
            # Bytewise addition of whole rows: add the low 7 bits of each
            # byte without carries crossing bytes, then fix the top bits.
            a, b = int.from_bytes(line, "big"), int.from_bytes(prior, "big")
            line = bytearray((((a & low) + (b & low)) ^ ((a ^ b) & high))
                             .to_bytes(stride, "big"))
        elif kind in (3, 4):
            for i in range(stride):
                a = line[i - channels] if i >= channels else 0
                b = prior[i]
                if kind == 3:
                    line[i] = (line[i] + (a + b) // 2) & 0xFF
                else:
                    c = prior[i - channels] if i >= channels else 0
                    p = a + b - c
                    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                    pred = a if pa <= pb and pa <= pc else b if pb <= pc else c
                    line[i] = (line[i] + pred) & 0xFF
        elif kind != 0:
            raise RuntimeError(f"PNG row {y}: unknown filter {kind}")
        rows.append(bytes(line))
        prior = line
    return width, height, rows


def check_screenshot(shot: dict, data: bytes) -> None:
    """Checks a screenshot PNG matches its reply's size and is not blank."""
    if not data:
        raise RuntimeError("screenshot blob is empty")
    width, height, rows = png_pixels(data)
    if (width, height) != (shot["width"], shot["height"]):
        raise RuntimeError(f"screenshot PNG is {width}x{height}, the reply "
                           f"says {shot['width']}x{shot['height']}")
    channels = len(rows[0]) // width
    first = rows[0][:channels]
    if all(row == first * width for row in rows):
        raise RuntimeError("screenshot is a single flat colour")


async def selftest(conn: Connection, methods: list[dict],
                   read_length: int | None, save: str | None) -> int:
    print("methods:", [m["name"] for m in methods])
    for m in tool_methods(methods):
        print(f"  tool {m['name'].replace('.', '_')}  params={[p['name'] for p in m.get('params', [])]}")
    wl = await conn.request("window.list")
    print("window.list:", wl)
    skipped = "the app has no window"
    if wl.get("windows"):
        wid = wl["windows"][0]["id"]
        intro = await conn.request("window.introspect", {"window": wid})
        print("introspect ok, root type:", intro["introspection"].get("role") or intro["introspection"].get("name"))
        # Paused, so both fetches see the same frame.
        await conn.request("app.pause")
        tree = await conn.request("window.renderTree",
                                  {"window": wid, "asBlob": True})
        data = await read_blob(conn, tree["blob"], read_length)
        inline = await conn.request("window.renderTree", {"window": wid})
        if json.loads(data) != inline:
            raise RuntimeError("renderTree blob differs from the inline reply")
        print("renderTree blob ok,", len(data), "bytes, same as the inline reply")
        if "window.screenshot" not in {m["name"] for m in methods}:
            skipped = "the app has no window.screenshot"
        elif not wl["windows"][0].get("canCapture"):
            skipped = "the window cannot capture"
        else:
            skipped = None
            shot = await conn.request("window.screenshot", {"window": wid})
            data = await read_blob(conn, shot["blob"], read_length)
            check_screenshot(shot, data)
            print(f"screenshot ok, {shot['width']}x{shot['height']},",
                  len(data), "bytes")
            if save:
                with open(save, "wb") as f:
                    f.write(data)
                print("screenshot saved to", save)
    if skipped:
        if save:
            raise RuntimeError(f"--save-screenshot: no screenshot, {skipped}")
        print("screenshot skipped:", skipped)
    print("SELFTEST OK")
    return 0


async def amain(args: argparse.Namespace) -> int:
    log_to_stderr()
    conn = Connection(on_notification=log_notification)
    try:
        await conn.launch()
        described = await conn.request("system.describe")
        methods = described.get("methods", [])
        read_length = blob_read_length(described)
        if args.selftest:
            return await selftest(conn, methods, read_length,
                                  args.save_screenshot)
        await run_mcp(conn, methods, read_length)
        return 0
    finally:
        await conn.close()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="MCP bridge for a bqui app.")
    parser.add_argument("--selftest", action="store_true",
                        help="drive the app once without MCP and exit")
    parser.add_argument("--save-screenshot", metavar="PATH",
                        help="with --selftest: write its screenshot here")
    args = parser.parse_args()
    if args.save_screenshot and not args.selftest:
        parser.error("--save-screenshot needs --selftest")
    return args


def main() -> None:
    args = parse_args()
    try:
        sys.exit(asyncio.run(amain(args)))
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
