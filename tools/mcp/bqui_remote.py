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
import binascii
import glob
import json
import logging
import os
import shlex
import struct
import threading
from typing import Any, Callable, Sequence

LENGTH_PREFIX = 4
MAX_FRAME = 256 << 20

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
    """The connection is gone; pending and later requests fail with it.

    `reason` says why: the app disconnected, it broke the protocol, or the
    connection was closed.
    """

    def __init__(self, reason: str = "app disconnected") -> None:
        super().__init__(reason)
        self.reason = reason


class _ProtocolError(Exception):
    pass


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

    `on_notification(method, params)` receives the app's notifications; an
    exception it raises is logged and does not affect the connection. A frame
    that breaks the protocol fails the connection like a disconnect.
    Requests before the app connects raise ConnectionError.
    """

    def __init__(self, on_notification: NotificationHandler | None = None,
                 max_frame: int = MAX_FRAME) -> None:
        self.endpoint: str | None = None
        self._on_notification = on_notification
        self._max_frame = max_frame
        self._reader: asyncio.StreamReader | None = None
        self._writer: asyncio.StreamWriter | None = None
        self._read_task: asyncio.Task | None = None
        self._next_id = 1
        self._pending: dict[int, asyncio.Future] = {}
        self._proc: asyncio.subprocess.Process | None = None
        self._server: asyncio.AbstractServer | None = None
        self._connected: asyncio.Future | None = None
        self._closing = False
        self._closed = False
        self._lost: str | None = None

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
        launch_command() and launch_env(). On failure the app is stopped and
        the listener closed before the error is raised.
        """
        try:
            if self.endpoint is None:
                await self.listen()
            cmd = list(command) if command is not None else launch_command(self.endpoint)
            log.info("launching: %s", " ".join(cmd))
            self._proc = await asyncio.create_subprocess_exec(
                *cmd, stdout=asyncio.subprocess.DEVNULL,
                env=env if env is not None else launch_env())
            exited = asyncio.ensure_future(self._proc.wait())
            try:
                done, _ = await asyncio.wait({self._connected, exited},
                                             timeout=timeout,
                                             return_when=asyncio.FIRST_COMPLETED)
            finally:
                exited.cancel()
                await asyncio.wait({exited})
            if self._connected not in done:
                if exited in done:
                    raise RuntimeError("bqui app exited with code "
                                       f"{self._proc.returncode} before connecting")
                raise RuntimeError(f"bqui app did not connect within {timeout:g}s")
        except BaseException:
            await self._stop_app(0)
            await self._close_server()
            raise

    async def wait_connected(self, timeout: float = 30.0) -> None:
        """Waits for an app started some other way to connect."""
        if self._connected is None:
            raise RuntimeError("wait_connected() before listen()")
        try:
            await asyncio.wait_for(asyncio.shield(self._connected), timeout)
        except asyncio.TimeoutError:
            raise TimeoutError(f"bqui app did not connect within {timeout:g}s") from None

    async def _on_connected(self, reader: asyncio.StreamReader,
                            writer: asyncio.StreamWriter) -> None:
        if self._reader is not None or self._closing:
            writer.close()
            return
        self._reader, self._writer = reader, writer
        self._read_task = asyncio.create_task(self._read_loop())
        log.info("app connected")
        if not self._connected.done():
            self._connected.set_result(True)

    async def _read_loop(self) -> None:
        reason = "connection closed"
        try:
            while True:
                header = await self._reader.readexactly(LENGTH_PREFIX)
                (length,) = struct.unpack(">I", header)
                if not 0 < length <= self._max_frame:
                    raise _ProtocolError(f"frame length {length}")
                payload = await self._reader.readexactly(length)
                try:
                    msg = json.loads(payload.decode("utf-8"))
                except ValueError as e:
                    raise _ProtocolError(f"bad frame: {e}") from None
                if not isinstance(msg, dict):
                    raise _ProtocolError("frame is not a JSON object")
                self._on_message(msg)
        except (asyncio.IncompleteReadError, ConnectionError):
            # A read failure during our own teardown is the app exiting on
            # request, not a fault; only an unexpected drop is worth logging.
            reason = "app disconnected"
            if not self._closing:
                log.warning(reason)
        except _ProtocolError as e:
            reason = f"protocol error: {e}"
            log.error(reason)
        except Exception as e:
            reason = f"reader failed: {e!r}"
            log.exception("reader failed")
        finally:
            self._fail(reason)

    def _fail(self, reason: str) -> None:
        if self._lost is None:
            self._lost = reason
        pending, self._pending = self._pending, {}
        for fut in pending.values():
            if not fut.done():
                fut.set_exception(ConnectionLost(reason))
        self._writer.close()

    def _on_message(self, msg: dict) -> None:
        if "method" in msg:
            if msg.get("id") is not None:
                log.warning("ignoring the app's request %r", msg["method"])
            elif self._on_notification:
                try:
                    self._on_notification(msg["method"], msg.get("params", {}))
                except Exception:
                    log.exception("notification handler failed on %r",
                                  msg["method"])
        elif "result" in msg or "error" in msg:
            fut = self._pending.pop(msg.get("id"), None)
            if fut is None:
                log.debug("dropping a reply to unknown id %r", msg.get("id"))
            elif not fut.done():
                fut.set_result(msg)
        else:
            raise _ProtocolError("message is neither a reply nor a notification")

    async def request(self, method: str, params: dict | None = None) -> Any:
        """Calls `method` and returns its result.

        Raises RemoteError for an error reply and ConnectionLost when the
        connection is gone.
        """
        if self._lost is not None:
            raise ConnectionLost(self._lost)
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
            resp = await fut
        except ConnectionLost:
            raise
        except ConnectionError:
            raise ConnectionLost(self._lost or "app disconnected") from None
        finally:
            self._pending.pop(rid, None)
        if "error" in resp:
            e = resp["error"]
            raise RemoteError(method, e.get("code"), e.get("message", ""),
                              e.get("data"))
        return resp.get("result")

    async def close(self, timeout: float = 5.0) -> None:
        """Asks the app to shut down, then stops it and the listener.

        Each step waits at most `timeout` seconds: the shutdown reply, the
        app's exit, and its exit after terminate; then it is killed. A second
        call does nothing.
        """
        if self._closed:
            return
        self._closed = self._closing = True
        shut_down = False
        if self._writer and self._lost is None:
            try:
                await asyncio.wait_for(self.request("app.shutdown"), timeout)
                shut_down = True
            except ConnectionError as e:
                log.debug("app.shutdown failed: %s", e)
            except asyncio.TimeoutError:
                log.warning("app.shutdown got no reply within %gs", timeout)
            except RemoteError as e:
                log.warning("%s", e)
        await self._stop_app(timeout if shut_down else 0, timeout)
        if self._writer:
            self._writer.close()
            try:
                await asyncio.wait_for(self._writer.wait_closed(), timeout)
            except (asyncio.TimeoutError, ConnectionError):
                pass
        if self._read_task:
            self._read_task.cancel()
            await asyncio.wait({self._read_task})
        await self._close_server()

    async def _stop_app(self, grace: float, timeout: float = 5.0) -> None:
        """Waits `grace` seconds for the app to exit, then terminates it."""
        proc = self._proc
        if proc is None or proc.returncode is not None:
            return
        if grace > 0:
            try:
                await asyncio.wait_for(proc.wait(), grace)
                return
            except asyncio.TimeoutError:
                log.warning("app did not exit within %gs, terminating", grace)
        for stop in (proc.terminate, proc.kill):
            try:
                stop()
            except ProcessLookupError:
                pass
            try:
                await asyncio.wait_for(proc.wait(), timeout)
                return
            except asyncio.TimeoutError:
                pass
        log.error("app process %s did not exit", proc.pid)

    async def _close_server(self) -> None:
        if self._server:
            self._server.close()
            await self._server.wait_closed()


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
            try:
                piece = base64.b64decode(chunk["data"], validate=True)
            except binascii.Error as e:
                raise RuntimeError(f"blob {blob['id']}: bad base64: {e}") from None
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
        except ConnectionLost as e:
            log.debug("blob %s: release failed: %s", blob["id"], e)
        except Exception as e:
            log.warning("blob %s: release failed: %s", blob["id"], e)


async def _within(coro: Any, timeout: float | None, what: str) -> Any:
    if timeout is None:
        return await coro
    try:
        return await asyncio.wait_for(coro, timeout)
    except asyncio.TimeoutError:
        raise TimeoutError(f"{what} timed out after {timeout:g}s") from None


class SyncConnection:
    """A blocking Connection for synchronous callers such as Robot Framework.

    Runs the Connection on a private event loop thread. `on_notification` is
    called on that thread and must not call back into this SyncConnection:
    the call would wait on the thread it blocks. Timeouts raise TimeoutError
    and leave the connection usable. After close(), calls raise
    ConnectionLost and close() does nothing. Also a context manager.
    """

    def __init__(self, on_notification: NotificationHandler | None = None) -> None:
        self.connection = Connection(on_notification)
        self._closed = False
        self._loop = asyncio.new_event_loop()
        self._thread = threading.Thread(target=self._loop.run_forever,
                                        name="bqui_remote", daemon=True)
        self._thread.start()

    def __enter__(self) -> SyncConnection:
        return self

    def __exit__(self, *exc: Any) -> None:
        self.close()

    def _call(self, make: Callable[[], Any]) -> Any:
        if self._closed:
            raise ConnectionLost("connection closed")
        return asyncio.run_coroutine_threadsafe(make(), self._loop).result()

    def listen(self, host: str = "127.0.0.1") -> str:
        """Connection.listen."""
        return self._call(lambda: self.connection.listen(host))

    def launch(self, command: Sequence[str] | None = None,
               env: dict[str, str] | None = None, timeout: float = 30.0) -> None:
        """Connection.launch."""
        self._call(lambda: self.connection.launch(command, env, timeout))

    def wait_connected(self, timeout: float = 30.0) -> None:
        """Connection.wait_connected."""
        self._call(lambda: self.connection.wait_connected(timeout))

    def request(self, method: str, params: dict | None = None,
                timeout: float | None = None) -> Any:
        """Connection.request, giving up after `timeout` seconds if set."""
        return self._call(lambda: _within(
            self.connection.request(method, params), timeout, method))

    def read_blob(self, blob: dict, length: int | None = None,
                  timeout: float | None = None) -> bytes:
        """read_blob, giving up after `timeout` seconds if set."""
        return self._call(lambda: _within(
            read_blob(self.connection, blob, length), timeout, f"blob {blob['id']}"))

    def close(self, timeout: float = 5.0) -> None:
        """Connection.close, then stops the loop thread."""
        if self._closed:
            return
        try:
            self._call(lambda: self.connection.close(timeout))
        finally:
            self._closed = True
            self._loop.call_soon_threadsafe(self._loop.stop)
            self._thread.join()
            self._loop.close()
