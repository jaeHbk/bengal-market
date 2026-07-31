#!/usr/bin/env python3

import base64
import hashlib
import json
import pathlib
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time
from typing import Optional


def receive_exact(connection: socket.socket, length: int) -> bytes:
    result = bytearray()
    while len(result) < length:
        chunk = connection.recv(length - len(result))
        if not chunk:
            raise RuntimeError("client closed WebSocket")
        result.extend(chunk)
    return bytes(result)


def receive_frame(connection: socket.socket) -> bytes:
    header = receive_exact(connection, 2)
    length = header[1] & 0x7F
    if length == 126:
        length = struct.unpack("!H", receive_exact(connection, 2))[0]
    elif length == 127:
        length = struct.unpack("!Q", receive_exact(connection, 8))[0]
    mask = receive_exact(connection, 4) if header[1] & 0x80 else b""
    payload = bytearray(receive_exact(connection, length))
    if mask:
        for index in range(length):
            payload[index] ^= mask[index % 4]
    return bytes(payload)


def send_text(connection: socket.socket, value: dict) -> None:
    payload = json.dumps(value, separators=(",", ":")).encode()
    if len(payload) < 126:
        header = bytes((0x81, len(payload)))
    elif len(payload) <= 0xFFFF:
        header = bytes((0x81, 126)) + struct.pack("!H", len(payload))
    else:
        header = bytes((0x81, 127)) + struct.pack("!Q", len(payload))
    connection.sendall(header + payload)


class WebSocketServer:
    def __init__(self) -> None:
        self.listener = socket.socket()
        self.listener.bind(("127.0.0.1", 0))
        self.listener.listen(1)
        self.port = self.listener.getsockname()[1]
        self.sent = threading.Event()
        self.done = threading.Event()
        self.error: Optional[BaseException] = None
        self.thread = threading.Thread(target=self.serve, daemon=True)

    def start(self) -> None:
        self.thread.start()

    def serve(self) -> None:
        try:
            connection, _ = self.listener.accept()
            with connection:
                request = bytearray()
                while b"\r\n\r\n" not in request:
                    request.extend(connection.recv(4096))
                headers = {}
                for line in request.decode().split("\r\n")[1:]:
                    if ":" in line:
                        name, value = line.split(":", 1)
                        headers[name.lower()] = value.strip()
                accept = base64.b64encode(
                    hashlib.sha1(
                        (
                            headers["sec-websocket-key"]
                            + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
                        ).encode()
                    ).digest()
                ).decode()
                connection.sendall(
                    (
                        "HTTP/1.1 101 Switching Protocols\r\n"
                        "Upgrade: websocket\r\n"
                        "Connection: Upgrade\r\n"
                        f"Sec-WebSocket-Accept: {accept}\r\n\r\n"
                    ).encode()
                )
                receive_frame(connection)
                receive_frame(connection)
                send_text(
                    connection,
                    {
                        "channel": "market_trades",
                        "sequence_num": 1,
                        "events": [
                            {
                                "type": "update",
                                "trades": [
                                    {
                                        "trade_id": "1",
                                        "product_id": "BTC-USD",
                                        "price": "30000",
                                        "size": "0.1",
                                        "side": "BUY",
                                        "time": "2026-01-01T00:00:00Z",
                                    }
                                ],
                            }
                        ],
                    },
                )
                self.sent.set()
                sequence = 2
                while not self.done.wait(0.05):
                    send_text(
                        connection,
                        {
                            "channel": "heartbeats",
                            "sequence_num": sequence,
                            "events": [],
                        },
                    )
                    sequence += 1
        except (BrokenPipeError, ConnectionResetError):
            pass
        except BaseException as error:
            self.error = error
            self.sent.set()
        finally:
            self.listener.close()

    def close(self) -> None:
        self.done.set()
        self.thread.join(timeout=3)
        if self.error:
            raise self.error


def main() -> int:
    binary = pathlib.Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="bengal-market-capture-") as root:
        directory = pathlib.Path(root)
        first_output = None
        for stop_signal in (signal.SIGINT, signal.SIGTERM):
            server = WebSocketServer()
            server.start()
            output = directory / f"capture-{stop_signal}.jsonl"
            if first_output is None:
                first_output = output
            process = subprocess.Popen(
                [
                    str(binary),
                    "capture",
                    "--output",
                    str(output),
                    "--product",
                    "BTC-USD",
                    "--duration",
                    "30",
                    "--endpoint",
                    f"ws://127.0.0.1:{server.port}",
                ],
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
            )
            assert server.sent.wait(timeout=5)
            if server.error:
                raise server.error
            partial = pathlib.Path(str(output) + ".part")
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline:
                if partial.exists() and partial.stat().st_size > 200:
                    break
                time.sleep(0.02)
            else:
                raise AssertionError(
                    "capture did not write a partial recording"
                )

            process.send_signal(stop_signal)
            stdout, stderr = process.communicate(timeout=5)
            assert process.returncode == 128 + stop_signal, stderr
            status = json.loads(stdout)
            assert status["interrupted"] is True
            assert status["signal"] == stop_signal
            assert status["frames"] >= 1
            assert output.is_file()
            assert not partial.exists()

            replay = subprocess.run(
                [
                    str(binary),
                    "replay",
                    "--input",
                    str(output),
                    "--engine",
                    "bengal",
                ],
                check=True,
                text=True,
                capture_output=True,
            )
            report = json.loads(replay.stdout)
            assert report["input"]["parse_errors"] == 0
            assert report["pipeline"]["events"] == 1
            server.close()

        assert first_output is not None
        collision = subprocess.run(
            [
                str(binary),
                "capture",
                "--output",
                str(first_output),
                "--duration",
                "1",
                "--endpoint",
                "ws://127.0.0.1:1",
            ],
            text=True,
            capture_output=True,
        )
        assert collision.returncode == 2
        assert "already exists" in collision.stderr
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
