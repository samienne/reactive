"""Client for the bqui inspector protocol.

Speaks JSON-RPC 2.0 to one bqui app over a stream framed by a 4-byte
big-endian length prefix. The app connects to an endpoint the client listens
on, so a session is: listen, launch the app with the endpoint, wait for it to
connect, then make requests. Needs only the standard library.

Messages go to the "bqui_remote" logger.

Launch defaults come from the environment:
  BQUI_BUILD_DIR     Meson build dir. Its *.dll directories are put on PATH so
                     the app's sibling DLLs resolve. Required unless BQUI_LAUNCH.
  BQUI_INSPECTORAPP  Path to the app exe (default: <build>/src/inspectorapp/
                     inspectorapp.exe, or inspectorapp on POSIX).
  BQUI_LAUNCH        Full launch command template overriding the above;
                     "{endpoint}" is substituted. PATH is still augmented from
                     BQUI_BUILD_DIR if set.
"""
from __future__ import annotations

import asyncio
import base64
import concurrent.futures
import glob
import json
import logging
import os
import shlex
import struct
import threading
from typing import Any, Callable, Sequence

LENGTH_PREFIX = 4

log = logging.getLogger("bqui_remote")

NotificationHandler = Callable[[str, Any], None]


class RemoteError(RuntimeError):
    """A JSON-RPC error reply."""

    def __init__(self, method: str, code: int | None, message: str,
                 data: Any = None) -> None:
        super().__init__(f"{method} failed [{code}]: {message}")
        self.method = method
        self.code = code
        self.message = message
        self.data = data


class ConnectionLost(ConnectionError):
    """The app's connection dropped; pending and later requests fail with it."""

    def __init__(self) -> None:
        super().__init__("app disconnected")


def launch_env() -> dict[str, str]:
    """The environment to launch the app in: PATH extended from BQUI_BUILD_DIR."""
    env = dict(os.environ)
    build = env.get("BQUI_BUILD_DIR")
    if build:
        dll_dirs = sorted({os.path.dirname(p)
                           for p in glob.glob(os.path.join(build, "**", "*.dll"),
                                              recursive=True)})
        env["PATH"] = os.pathsep.join(dll_dirs + [env.get("PATH", "")])
    return env


def launch_command(endpoint: str) -> list[str]:
    """The command that launches the app pointed at `endpoint`."""
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


class Connection:
    """The framed JSON-RPC stream to one bqui app.

    `on_notification(method, params)` receives the app's notifications.
    """

    def __init__(self, on_notification: NotificationHandler | None = None) -> None:
        self.endpoint: str | None = None
        self._on_notification = on_notification
        self._reader: asyncio.StreamReader | None = None
        self._writer: asyncio.StreamWriter | None = None
        self._next_id = 1
        self._pending: dict[int, asyncio.Future] = {}
        self._proc: asyncio.subprocess.Process | None = None
        self._server: asyncio.AbstractServer | None = None
        self._connected: asyncio.Future | None = None
        self._closing = False
        self._lost = False

    async def listen(self, host: str = "127.0.0.1") -> str:
        """Listens on a free loopback port and returns its endpoint."""
        self._connected = asyncio.get_running_loop().create_future()
        # Keep the listener open for the connection's lifetime: closing it
        # right after accept resets the accepted socket on Windows Proactor.
        self._server = await asyncio.start_server(self._on_connected, host, 0)
        port = self._server.sockets[0].getsockname()[1]
        self.endpoint = f"tcp://{host}:{port}"
        log.info("listening on %s", self.endpoint)
        return self.endpoint

    async def launch(self, command: Sequence[str] | None = None,
                     env: dict[str, str] | None = None,
                     timeout: float = 30.0) -> None:
        """Launches the app and waits for it to connect.

        Listens first if needed. `command` and `env` default to
        launch_command() and launch_env().
        """
        if self.endpoint is None:
            await self.listen()
        cmd = list(command) if command is not None else launch_command(self.endpoint)
        log.info("launching: %s", " ".join(cmd))
        self._proc = await asyncio.create_subprocess_exec(
            *cmd, stdout=asyncio.subprocess.DEVNULL,
            env=env if env is not None else launch_env())

        exited = asyncio.ensure_future(self._proc.wait())
        done, _ = await asyncio.wait({self._connected, exited}, timeout=timeout,
                                     return_when=asyncio.FIRST_COMPLETED)
        if self._connected not in done:
            exited.cancel()
            if exited in done:
                raise RuntimeError("bqui app exited with code "
                                   f"{self._proc.returncode} before connecting")
            raise RuntimeError(f"bqui app did not connect within {timeout:g}s")

    async def wait_connected(self, timeout: float = 30.0) -> None:
        """Waits for an app started some other way to connect."""
        await asyncio.wait_for(asyncio.shield(self._connected), timeout)

    async def _on_connected(self, reader: asyncio.StreamReader,
                            writer: asyncio.StreamWriter) -> None:
        if self._reader is not None:
            writer.close()
            return
        self._reader, self._writer = reader, writer
        asyncio.create_task(self._read_loop())
        log.info("app connected")
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
            # A read failure during our own teardown is the app exiting on
            # request, not a fault; only an unexpected drop is worth logging.
            if not self._closing:
                log.warning("app disconnected")
            self._lost = True
            pending, self._pending = self._pending, {}
            for fut in pending.values():
                if not fut.done():
                    fut.set_exception(ConnectionLost())

    def _on_message(self, msg: dict) -> None:
        if msg.get("id") is not None and ("result" in msg or "error" in msg):
            fut = self._pending.pop(msg["id"], None)
            if fut and not fut.done():
                fut.set_result(msg)
        elif "method" in msg and self._on_notification:
            self._on_notification(msg["method"], msg.get("params", {}))

    async def request(self, method: str, params: dict | None = None) -> Any:
        """Calls `method` and returns its result.

        Raises RemoteError for an error reply and ConnectionLost when the
        connection drops.
        """
        if self._lost:
            raise ConnectionLost()
        if self._writer is None:
            raise ConnectionError("app not connected")
        rid = self._next_id
        self._next_id += 1
        frame = {"jsonrpc": "2.0", "id": rid, "method": method}
        if params:
            frame["params"] = params
        fut = asyncio.get_running_loop().create_future()
        self._pending[rid] = fut
        data = json.dumps(frame).encode("utf-8")
        try:
            self._writer.write(struct.pack(">I", len(data)) + data)
            await self._writer.drain()
        except ConnectionError:
            self._pending.pop(rid, None)
            raise ConnectionLost() from None
        resp = await fut
        if "error" in resp:
            e = resp["error"]
            raise RemoteError(method, e.get("code"), e.get("message", ""),
                              e.get("data"))
        return resp.get("result")

    async def close(self) -> None:
        """Asks the app to shut down, then stops it and the listener."""
        self._closing = True
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
        if self._writer:
            self._writer.close()


def blob_read_length(described: dict) -> int | None:
    """The largest `blob.read` length a `system.describe` reply allows."""
    return described.get("limits", {}).get("blob", {}).get("maxRead")


async def read_blob(conn: Connection, blob: dict,
                    length: int | None = None) -> bytes:
    """Pulls a blob's bytes in `blob.read` chunks, then releases it."""
    size = blob["size"]
    data = bytearray()
    try:
        while True:
            params = {"id": blob["id"], "offset": len(data)}
            if length:
                params["length"] = length
            chunk = await conn.request("blob.read", params)
            piece = base64.b64decode(chunk["data"])
            if not piece and not chunk["end"]:
                raise RuntimeError(f"blob {blob['id']}: a read made no progress")
            data += piece
            if len(data) > size or (chunk["end"] and len(data) != size):
                raise RuntimeError(f"blob {blob['id']}: read {len(data)} bytes "
                                   f"of a {size}-byte blob")
            if chunk["end"]:
                return bytes(data)
    finally:
        # Best effort: the pull's own outcome is what the caller gets.
        try:
            await conn.request("blob.release", {"id": blob["id"]})
        except Exception as e:
            log.warning("blob %s: release failed: %s", blob["id"], e)


class SyncConnection:
    """A blocking Connection for synchronous callers such as Robot Framework.

    Runs the Connection on a private event loop thread, which is also where
    `on_notification` is called.
    """

    def __init__(self, on_notification: NotificationHandler | None = None) -> None:
        self.connection = Connection(on_notification)
        self._loop = asyncio.new_event_loop()
        self._thread = threading.Thread(target=self._loop.run_forever,
                                        name="bqui_remote", daemon=True)
        self._thread.start()

    def _call(self, coro: Any, timeout: float | None = None) -> Any:
        future = asyncio.run_coroutine_threadsafe(coro, self._loop)
        try:
            return future.result(timeout)
        except concurrent.futures.TimeoutError:
            future.cancel()
            raise

    def listen(self, host: str = "127.0.0.1") -> str:
        return self._call(self.connection.listen(host))

    def launch(self, command: Sequence[str] | None = None,
               env: dict[str, str] | None = None, timeout: float = 30.0) -> None:
        self._call(self.connection.launch(command, env, timeout))

    def wait_connected(self, timeout: float = 30.0) -> None:
        self._call(self.connection.wait_connected(timeout))

    def request(self, method: str, params: dict | None = None,
                timeout: float | None = None) -> Any:
        """Connection.request, giving up after `timeout` seconds if set."""
        return self._call(self.connection.request(method, params), timeout)

    def read_blob(self, blob: dict, length: int | None = None) -> bytes:
        return self._call(read_blob(self.connection, blob, length))

    def close(self) -> None:
        """Closes the connection and stops the loop thread."""
        try:
            self._call(self.connection.close())
        finally:
            self._loop.call_soon_threadsafe(self._loop.stop)
            self._thread.join()
            self._loop.close()
