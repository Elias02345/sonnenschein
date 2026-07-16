"""Sonnenschein Decky plugin backend.

Reuses the pairing of the installed Sonnenschein Client (its client
certificate + key) to talk to the host's cert-authenticated Moonlight
HTTPS API: app list (the unified library — desktop, Big Picture and all
installed Steam games) and box art. Streams are launched through the
Sonnenschein Client app itself.
"""

import base64
import glob
import http.client
import json
import os
import ssl
import tempfile
import urllib.parse
import uuid as uuidlib
import xml.etree.ElementTree as ET

import decky  # type: ignore

CLIENT_CONF = os.path.join(
    decky.DECKY_USER_HOME, ".config", "Sonnenschein", "sonnenschein-client.conf"
)
STATE_FILE = os.path.join(decky.DECKY_PLUGIN_SETTINGS_DIR, "state.json")
RUNNER = os.path.join(decky.DECKY_PLUGIN_DIR, "sonnenschein-run.sh")

HTTPS_PORT = 47984


def _unescape_qsettings(value: str) -> str:
    """Undo QSettings INI escaping for a quoted value."""
    if value.startswith('"') and value.endswith('"'):
        value = value[1:-1]
    return (
        value.replace("\\n", "\n")
        .replace("\\r", "\r")
        .replace('\\"', '"')
        .replace("\\\\", "\\")
    )


def _read_client_conf():
    """Parse the Sonnenschein Client QSettings INI: identity + paired hosts."""
    conf = {"certificate": None, "key": None, "uniqueid": None, "hosts": []}
    if not os.path.exists(CLIENT_CONF):
        return conf

    hosts = {}
    with open(CLIENT_CONF, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if "=" not in line or line.startswith("["):
                continue
            key, _, raw = line.partition("=")
            key = key.strip()
            raw = raw.strip()

            if key in ("certificate", "key"):
                val = _unescape_qsettings(raw)
                if val.startswith("@ByteArray("):
                    val = val[len("@ByteArray(") : -1]
                conf[key] = val
            elif key == "uniqueid":
                conf["uniqueid"] = _unescape_qsettings(raw)
            elif key.startswith("hosts\\"):
                # hosts\<n>\<field>=<value>
                parts = key.split("\\")
                if len(parts) == 3:
                    idx, field = parts[1], parts[2]
                    hosts.setdefault(idx, {})[field] = _unescape_qsettings(raw)

    for idx in sorted(hosts, key=lambda i: int(i) if i.isdigit() else 0):
        h = hosts[idx]
        address = h.get("manualaddress") or h.get("localaddress") or h.get("remoteaddress")
        if not address:
            continue
        conf["hosts"].append(
            {
                "name": h.get("name", address),
                "uuid": h.get("uuid", ""),
                "address": address,
                "port": int(h.get("localport") or h.get("manualport") or HTTPS_PORT),
            }
        )
    return conf


class _HostApi:
    """Minimal mutual-TLS client for the Moonlight HTTPS API."""

    def __init__(self, conf):
        self._conf = conf
        self._ctx = None
        self._tmpdir = None

    def _context(self):
        if self._ctx is not None:
            return self._ctx
        if not self._conf["certificate"] or not self._conf["key"]:
            raise RuntimeError("Sonnenschein Client identity not found — is the client installed and paired?")
        self._tmpdir = tempfile.mkdtemp(prefix="sonnenschein-decky-")
        cert_path = os.path.join(self._tmpdir, "client.pem")
        key_path = os.path.join(self._tmpdir, "client.key")
        with open(cert_path, "w") as f:
            f.write(self._conf["certificate"])
        fd = os.open(key_path, os.O_WRONLY | os.O_CREAT, 0o600)
        with os.fdopen(fd, "w") as f:
            f.write(self._conf["key"])
        ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
        # Moonlight hosts use self-signed certs; identity is proven by the
        # mutual-TLS client certificate from pairing.
        ctx.check_hostname = False
        ctx.verify_mode = ssl.CERT_NONE
        ctx.load_cert_chain(cert_path, key_path)
        self._ctx = ctx
        return ctx

    def get(self, address, port, path):
        conn = http.client.HTTPSConnection(address, port, timeout=10, context=self._context())
        try:
            conn.request("GET", path, headers={"User-Agent": "sonnenschein-decky"})
            resp = conn.getresponse()
            body = resp.read()
            if resp.status != 200:
                raise RuntimeError(f"Host returned HTTP {resp.status} for {path}")
            return body
        finally:
            conn.close()


class Plugin:
    async def _main(self):
        decky.logger.info("Sonnenschein plugin loaded")

    async def _unload(self):
        decky.logger.info("Sonnenschein plugin unloaded")

    # ---- state (shortcut appid mapping) ----

    async def get_state(self):
        try:
            with open(STATE_FILE, "r") as f:
                return json.load(f)
        except Exception:
            return {}

    async def set_state(self, state):
        os.makedirs(os.path.dirname(STATE_FILE), exist_ok=True)
        with open(STATE_FILE, "w") as f:
            json.dump(state, f)
        return True

    # ---- host access ----

    async def get_status(self):
        conf = _read_client_conf()
        return {
            "clientConfFound": os.path.exists(CLIENT_CONF),
            "paired": bool(conf["certificate"]) and bool(conf["hosts"]),
            "clientAppFound": bool(_find_client_app()),
            "runnerPath": RUNNER,
            "hosts": conf["hosts"],
        }

    async def get_apps(self, address, port):
        """Fetch the host's app list (unified library) via mutual TLS."""
        conf = _read_client_conf()
        api = _HostApi(conf)
        uid = conf["uniqueid"] or "0123456789ABCDEF"
        path = "/applist?uniqueid={}&uuid={}".format(
            urllib.parse.quote(uid), uuidlib.uuid4().hex
        )
        body = api.get(address, int(port), path)

        apps = []
        root = ET.fromstring(body)
        for app in root.iter("App"):
            title = app.findtext("AppTitle")
            app_id = app.findtext("ID")
            if title and app_id:
                apps.append({"id": app_id, "title": title})
        return apps

    async def get_boxart(self, address, port, app_id):
        """Return base64 box art for an app, or empty string if unavailable."""
        conf = _read_client_conf()
        api = _HostApi(conf)
        uid = conf["uniqueid"] or "0123456789ABCDEF"
        path = "/appasset?uniqueid={}&uuid={}&appid={}&AssetType=2&AssetIdx=0".format(
            urllib.parse.quote(uid), uuidlib.uuid4().hex, urllib.parse.quote(str(app_id))
        )
        try:
            body = api.get(address, int(port), path)
        except Exception as e:
            decky.logger.info(f"No box art for app {app_id}: {e}")
            return {"data": "", "ext": ""}
        ext = "png" if body[:8] == b"\x89PNG\r\n\x1a\n" else "jpg"
        return {"data": base64.b64encode(body).decode("ascii"), "ext": ext}

    async def get_launch_options(self, address, app_title):
        """Launch options string encoding host + app for the runner script."""
        host = address.replace("'", "")
        app = app_title.replace("'", "")
        return f"SONNENSCHEIN_HOST='{host}' SONNENSCHEIN_APP='{app}' %command%"


def _find_client_app():
    """Locate the Sonnenschein Client on the Deck (AppImage or flatpak)."""
    candidates = sorted(
        glob.glob(os.path.join(decky.DECKY_USER_HOME, "Applications", "Sonnenschein_Client*.AppImage"))
    )
    if candidates:
        return candidates[-1]
    for flatpak in ("/var/lib/flatpak", os.path.join(decky.DECKY_USER_HOME, ".local/share/flatpak")):
        if glob.glob(os.path.join(flatpak, "app", "io.github.elias02345.Sonnenschein*")):
            return "flatpak"
    return ""
