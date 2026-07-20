#!/usr/bin/env python3
"""Dependency-free regression harness for the Decky Python backend.

Runs with system Python and a minimal injected ``decky`` module. It verifies
that blocking host I/O is dispatched away from Decky's asyncio event loop and
that the backend remains importable when ``xml.etree`` is unavailable in the
Loader's frozen Python runtime.
"""

import asyncio
import importlib.abc
import importlib.util
import logging
import os
from pathlib import Path
import sys
import tempfile
import time
import types


class _NoXmlEtree(importlib.abc.MetaPathFinder):
    def find_spec(self, fullname, path, target=None):
        if fullname == "xml.etree" or fullname.startswith("xml.etree."):
            raise ModuleNotFoundError("xml.etree blocked by frozen-runtime harness")
        return None


async def _run(module):
    plugin = module.Plugin()

    status = await plugin.get_status()
    assert status["paired"] is True
    assert status["clientAppFound"] is True
    assert status["hosts"] == [
        {
            "name": "deck-test-host",
            "uuid": "host-uuid",
            "address": "192.0.2.10",
            "port": 47989,
        }
    ]

    def slow_apps(address, port):
        time.sleep(0.35)
        return [{"id": "220", "title": "Half-Life 2"}]

    def slow_status(address, port, selected_app_id):
        time.sleep(0.35)
        return True, False

    plugin._get_apps_sync = slow_apps
    real_probe_host_state = module._probe_host_state
    module._probe_host_state = slow_status

    started = time.monotonic()
    apps_task = asyncio.create_task(plugin.get_apps("offline.invalid", 47989))
    status_task = asyncio.create_task(plugin.check_host_available("offline.invalid", 47989, "220"))

    # These two operations are intentionally still running in worker threads.
    # A healthy Decky event loop must answer ping immediately in parallel.
    await asyncio.sleep(0.02)
    ping_started = time.monotonic()
    assert await plugin.ping() is True
    ping_elapsed = time.monotonic() - ping_started
    assert ping_elapsed < 0.1, f"event loop blocked for {ping_elapsed:.3f}s"

    apps, status = await asyncio.gather(apps_task, status_task)
    elapsed = time.monotonic() - started
    assert apps == [{"id": "220", "title": "Half-Life 2"}]
    assert status == {"reachable": True, "busy": False}
    assert elapsed < 0.65, f"host calls did not run concurrently ({elapsed:.3f}s)"
    module._probe_host_state = real_probe_host_state

    def fail():
        raise RuntimeError("worker failure")

    try:
        await module._run_blocking(fail)
    except RuntimeError as exc:
        assert str(exc) == "worker failure"
    else:
        raise AssertionError("worker exception was swallowed")

    # Busy means another game, not an already-running session of the selected
    # game. Exercise the real XML parser without touching the network.
    import http.client

    class Response:
        status = 200

        def __init__(self, body):
            self._body = body

        def read(self):
            return self._body

    class Connection:
        body = b""

        def __init__(self, *args, **kwargs):
            pass

        def request(self, *args, **kwargs):
            pass

        def getresponse(self):
            return Response(self.body)

        def close(self):
            pass

    original_connection = http.client.HTTPConnection
    http.client.HTTPConnection = Connection
    try:
        Connection.body = b"<root><state>SUNSHINE_SERVER_BUSY</state><currentgame>220</currentgame></root>"
        same_game = module._probe_host_state("host", 47989, "220")
        other_game = module._probe_host_state("host", 47989, "999")
        assert same_game == (True, False), same_game
        assert other_game == (True, True), other_game
        Connection.body = b"<root><state>SUNSHINE_SERVER_FREE</state><currentgame>0</currentgame></root>"
        assert module._probe_host_state("host", 47989, "220") == (True, False)
    finally:
        http.client.HTTPConnection = original_connection


def main():
    plugin_root = Path(__file__).resolve().parents[1]
    with tempfile.TemporaryDirectory(prefix="sonnenschein-decky-test-") as tmp:
        config_dir = Path(tmp, ".config", "Sonnenschein")
        config_dir.mkdir(parents=True)
        Path(config_dir, "sonnenschein-client.conf").write_text(
            """[General]
certificate=@ByteArray(TEST-CERT)
key=@ByteArray(TEST-KEY)
uniqueid=test-client

[hosts]
1\\hostname=deck-test-host
1\\uuid=host-uuid
1\\localaddress=192.0.2.10
1\\localport=47989
size=1
""",
            encoding="utf-8",
        )
        applications_dir = Path(tmp, "Applications")
        applications_dir.mkdir()
        Path(applications_dir, "Sonnenschein_Client-0.2.2-test-x86_64.AppImage").touch()

        decky = types.ModuleType("decky")
        decky.DECKY_USER_HOME = tmp
        decky.DECKY_PLUGIN_SETTINGS_DIR = os.path.join(tmp, "settings")
        decky.DECKY_PLUGIN_DIR = str(plugin_root)
        decky.logger = logging.getLogger("sonnenschein-decky-test")
        sys.modules["decky"] = decky
        sys.modules["decky_plugin"] = decky
        sys.meta_path.insert(0, _NoXmlEtree())

        spec = importlib.util.spec_from_file_location("sonnenschein_decky_backend", plugin_root / "main.py")
        assert spec and spec.loader
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        asyncio.run(_run(module))

    print("Decky backend regression harness: OK")


if __name__ == "__main__":
    main()
