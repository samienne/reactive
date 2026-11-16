"""Unit tests for bqui_remote against a fake app; no bqui build needed.

Run: python -m unittest discover -s tools/mcp
"""
from __future__ import annotations

import asyncio
import base64
import json
import struct
import sys
import threading
import unittest
from typing import Any, Callable

from bqui_remote import (Connection, ConnectionLost, RemoteError,
                         SyncConnection, read_blob)

Handler = Callable[[dict], dict]


class FakeApp:
    """The app's side of the framed stream."""

    def __init__(self, reader: asyncio.StreamReader,
                 writer: asyncio.StreamWriter) -> None:
        self.reader, self.writer = reader, writer

    @classmethod
    async def connect(cls, endpoint: str) -> FakeApp:
        host, port = endpoint.removeprefix("tcp://").rsplit(":", 1)
        return cls(*await asyncio.open_connection(host, int(port)))

    async def recv(self) -> dict:
        (length,) = struct.unpack(">I", await self.reader.readexactly(4))
        return json.loads(await self.reader.readexactly(length))

    async def send(self, msg: dict) -> None:
        await self.send_raw(json.dumps(msg).encode("utf-8"))

    async def send_raw(self, data: bytes) -> None:
        self.writer.write(struct.pack(">I", len(data)) + data)
        await self.writer.drain()

    async def reply(self, msg: dict, result: Any) -> None:
        await self.send({"jsonrpc": "2.0", "id": msg["id"], "result": result})

    async def close(self) -> None:
        self.writer.close()
        try:
            await self.writer.wait_closed()
        except ConnectionError:
            pass

    async def serve(self, handler: Handler) -> None:
        """Answers each request with handler's reply until app.shutdown."""
        while True:
            try:
                msg = await self.recv()
            except (asyncio.IncompleteReadError, ConnectionError):
                return
            if msg["method"] == "app.shutdown":
                await self.reply(msg, None)
                await self.close()
                return
            await self.send(handler(msg))


def result(msg: dict, value: Any) -> dict:
    return {"jsonrpc": "2.0", "id": msg["id"], "result": value}


class BlobStore:
    """Serves one blob in fixed-size chunks and records the calls made."""

    def __init__(self, data: bytes, chunk: int = 4, size: int | None = None,
                 empty_read: bool = False, fail_read: bool = False) -> None:
        self.data, self.chunk = data, chunk
        self.size = len(data) if size is None else size
        self.empty_read, self.fail_read = empty_read, fail_read
        self.offsets: list[int] = []
        self.released: list[str] = []
        self.descriptor = {"id": "b1", "size": self.size,
                           "contentType": "application/octet-stream"}

    def __call__(self, msg: dict) -> dict:
        p = msg.get("params", {})
        if msg["method"] == "blob.release":
            self.released.append(p["id"])
            return result(msg, {})
        if self.fail_read:
            return {"jsonrpc": "2.0", "id": msg["id"],
                    "error": {"code": -32001, "message": "no such blob"}}
        offset = p["offset"]
        self.offsets.append(offset)
        piece = b"" if self.empty_read else \
            self.data[offset:offset + min(p.get("length", 1 << 30), self.chunk)]
        end = not self.empty_read and offset + len(piece) >= len(self.data)
        return result(msg, {"data": base64.b64encode(piece).decode("ascii"),
                            "end": end})


class ConnectionTest(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self) -> None:
        self.notes: list[tuple[str, Any]] = []
        self.conn = Connection(lambda m, p: self.notes.append((m, p)))
        endpoint = await self.conn.listen()
        self.app = await FakeApp.connect(endpoint)
        await self.conn.wait_connected(5)

    async def asyncTearDown(self) -> None:
        await self.app.close()
        await self.conn.close()

    async def serve(self, handler: Handler) -> asyncio.Task:
        return asyncio.create_task(self.app.serve(handler))

    async def test_replies_out_of_order(self) -> None:
        first = asyncio.create_task(self.conn.request("a", {"n": 1}))
        second = asyncio.create_task(self.conn.request("b"))
        a, b = await self.app.recv(), await self.app.recv()
        self.assertEqual((a["method"], a["params"]), ("a", {"n": 1}))
        self.assertNotIn("params", b)
        await self.app.reply(b, "B")
        await self.app.reply(a, "A")
        self.assertEqual(await asyncio.wait_for(first, 5), "A")
        self.assertEqual(await asyncio.wait_for(second, 5), "B")

    async def test_notifications(self) -> None:
        pending = asyncio.create_task(self.conn.request("ping"))
        msg = await self.app.recv()
        await self.app.send({"jsonrpc": "2.0", "method": "log",
                             "params": {"message": "hi"}})
        await self.app.send({"jsonrpc": "2.0", "method": "frame"})
        await self.app.reply(msg, "pong")
        self.assertEqual(await asyncio.wait_for(pending, 5), "pong")
        self.assertEqual(self.notes, [("log", {"message": "hi"}), ("frame", {})])

    async def test_error_reply(self) -> None:
        await self.serve(lambda msg: {
            "jsonrpc": "2.0", "id": msg["id"],
            "error": {"code": -32602, "message": "bad window",
                      "data": {"window": 9}}})
        with self.assertRaises(RemoteError) as caught:
            await self.conn.request("window.list")
        e = caught.exception
        self.assertEqual((e.method, e.code, e.message, e.data),
                         ("window.list", -32602, "bad window", {"window": 9}))
        self.assertIn("window.list failed [-32602]: bad window", str(e))

    async def test_connection_lost_mid_request(self) -> None:
        pending = asyncio.create_task(self.conn.request("app.step"))
        await self.app.recv()
        with self.assertLogs("bqui_remote", "WARNING") as logs:
            await self.app.close()
            with self.assertRaises(ConnectionLost):
                await asyncio.wait_for(pending, 5)
        self.assertIn("app disconnected", logs.output[0])
        with self.assertRaises(ConnectionLost):
            await self.conn.request("window.list")

    async def test_blob_over_several_chunks(self) -> None:
        store = BlobStore(b"0123456789", chunk=4)
        await self.serve(store)
        data = await read_blob(self.conn, store.descriptor, length=4)
        self.assertEqual(data, b"0123456789")
        self.assertEqual(store.offsets, [0, 4, 8])
        self.assertEqual(store.released, ["b1"])

    async def test_blob_released_on_error(self) -> None:
        store = BlobStore(b"0123", fail_read=True)
        await self.serve(store)
        with self.assertRaises(RemoteError):
            await read_blob(self.conn, store.descriptor)
        self.assertEqual(store.released, ["b1"])

    async def test_blob_without_progress(self) -> None:
        store = BlobStore(b"0123", empty_read=True)
        await self.serve(store)
        with self.assertRaisesRegex(RuntimeError, "no progress"):
            await read_blob(self.conn, store.descriptor)
        self.assertEqual(store.released, ["b1"])

    async def test_blob_size_mismatch(self) -> None:
        store = BlobStore(b"0123", size=6)
        await self.serve(store)
        with self.assertRaisesRegex(RuntimeError, "read 4 bytes of a 6-byte"):
            await read_blob(self.conn, store.descriptor)
        self.assertEqual(store.released, ["b1"])


    async def test_raising_notification_handler_is_logged(self) -> None:
        def handler(method: str, params: Any) -> None:
            raise ValueError("boom")

        self.conn._on_notification = handler
        pending = asyncio.create_task(self.conn.request("ping"))
        msg = await self.app.recv()
        with self.assertLogs("bqui_remote", "ERROR") as logs:
            await self.app.send({"jsonrpc": "2.0", "method": "frame"})
            await self.app.reply(msg, "pong")
            self.assertEqual(await asyncio.wait_for(pending, 5), "pong")
        self.assertIn("notification handler failed", logs.output[0])

    async def test_graceful_close(self) -> None:
        seen: list[str] = []

        def handler(msg: dict) -> dict:
            seen.append(msg["method"])
            return result(msg, None)

        served = await self.serve(handler)
        await asyncio.wait_for(self.conn.close(timeout=5), 5)
        await asyncio.wait_for(served, 5)
        self.assertEqual(seen, [])
        self.assertFalse(self.conn._server.is_serving())

    async def test_close_with_silent_app(self) -> None:
        with self.assertLogs("bqui_remote", "WARNING") as logs:
            await asyncio.wait_for(self.conn.close(timeout=0.1), 5)
        self.assertIn("no reply", logs.output[0])
        self.assertEqual(self.conn._pending, {})
        await self.conn.close(timeout=0.1)


class ProtocolErrorTest(unittest.IsolatedAsyncioTestCase):
    """Each malformed frame fails the connection; nothing hangs."""

    CASES = {
        "bad JSON": b"{nope",
        "empty frame": b"",
        "not an object": b"[1]",
        "not UTF-8": b"\xff\xfe",
        "frame too long": b"x" * 17,
        "neither reply nor notification": b'{"jsonrpc": "2.0", "id": 1}',
    }

    async def test_malformed_frames(self) -> None:
        for name, frame in self.CASES.items():
            with self.subTest(name):
                conn = Connection(max_frame=16)
                app = await FakeApp.connect(await conn.listen())
                await conn.wait_connected(5)
                pending = asyncio.create_task(conn.request("x"))
                await app.recv()
                with self.assertLogs("bqui_remote", "ERROR"):
                    await app.send_raw(frame)
                    with self.assertRaisesRegex(ConnectionLost, "protocol error"):
                        await asyncio.wait_for(pending, 5)
                with self.assertRaisesRegex(ConnectionLost, "protocol error"):
                    await conn.request("y")
                await asyncio.wait_for(conn.close(timeout=1), 5)
                await app.close()


class LaunchTest(unittest.IsolatedAsyncioTestCase):
    def assertCleanedUp(self, conn: Connection) -> None:
        if conn._proc is not None:
            self.assertIsNotNone(conn._proc.returncode)
        self.assertFalse(conn._server.is_serving())

    async def test_app_exits_before_connecting(self) -> None:
        conn = Connection()
        with self.assertRaisesRegex(RuntimeError, "exited with code 3"):
            await conn.launch([sys.executable, "-c", "raise SystemExit(3)"],
                              timeout=30)
        self.assertCleanedUp(conn)
        await conn.close()

    async def test_timeout_stops_the_app(self) -> None:
        conn = Connection()
        with self.assertRaisesRegex(RuntimeError, "did not connect within 0.2s"):
            await conn.launch([sys.executable, "-c", "import time; time.sleep(30)"],
                              timeout=0.2)
        self.assertCleanedUp(conn)
        await conn.close()

    async def test_bad_path(self) -> None:
        conn = Connection()
        with self.assertRaises(OSError):
            await conn.launch([sys.executable + ".missing"], timeout=5)
        self.assertIsNone(conn._proc)
        self.assertCleanedUp(conn)
        await conn.close()

    async def test_close_terminates_a_silent_app(self) -> None:
        conn = Connection()
        endpoint = await conn.listen()
        host, port = endpoint.removeprefix("tcp://").rsplit(":", 1)
        await conn.launch([sys.executable, "-c",
                           "import socket, time; "
                           f"s = socket.create_connection(({host!r}, {port})); "
                           "time.sleep(30)"], timeout=30)
        with self.assertLogs("bqui_remote", "WARNING"):
            await asyncio.wait_for(conn.close(timeout=0.1), 10)
        self.assertCleanedUp(conn)


class SyncConnectionTest(unittest.TestCase):
    def start_app(self, endpoint: str, handler: Handler,
                  ignore_first: bool = False) -> threading.Thread:
        async def app() -> None:
            fake = await FakeApp.connect(endpoint)
            if ignore_first:
                await fake.recv()
            await fake.serve(handler)

        thread = threading.Thread(target=asyncio.run, args=(app(),))
        thread.start()
        self.addCleanup(thread.join, 5)
        return thread

    def test_request_timeout(self) -> None:
        with SyncConnection() as conn:
            self.start_app(conn.listen(), lambda msg: result(msg, "late"),
                           ignore_first=True)
            conn.wait_connected(5)
            with self.assertRaisesRegex(TimeoutError, "slow timed out after 0.1s"):
                conn.request("slow", timeout=0.1)
            self.assertEqual(conn.connection._pending, {})
            self.assertEqual(conn.request("next", timeout=5), "late")

    def test_use_after_close(self) -> None:
        conn = SyncConnection()
        self.start_app(conn.listen(), lambda msg: result(msg, None))
        conn.wait_connected(5)
        conn.close()
        with self.assertRaisesRegex(ConnectionLost, "closed"):
            conn.request("x")
        with self.assertRaises(ConnectionLost):
            conn.read_blob({"id": "b1", "size": 0})
        conn.close()

    def test_request_and_blob(self) -> None:
        conn = SyncConnection()
        endpoint = conn.listen()
        store = BlobStore(b"abcdefg", chunk=3)

        def handler(msg: dict) -> dict:
            if msg["method"].startswith("blob."):
                return store(msg)
            return result(msg, {"echo": msg.get("params")})

        async def app() -> None:
            await (await FakeApp.connect(endpoint)).serve(handler)

        thread = threading.Thread(target=asyncio.run, args=(app(),))
        thread.start()
        try:
            conn.wait_connected(5)
            self.assertEqual(conn.request("echo", {"x": 1}, timeout=5),
                             {"echo": {"x": 1}})
            self.assertEqual(conn.read_blob(store.descriptor), b"abcdefg")
            self.assertEqual(store.released, ["b1"])
        finally:
            conn.close()
            thread.join(5)
        self.assertFalse(thread.is_alive())


if __name__ == "__main__":
    unittest.main()
