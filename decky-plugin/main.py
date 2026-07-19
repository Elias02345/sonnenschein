"""Sonnenschein Decky plugin backend.

Reuses the pairing of the installed Sonnenschein Client (its client
certificate + key) to talk to the host's cert-authenticated Moonlight
HTTPS API: app list (the unified library — desktop, Big Picture and all
installed Steam games) and box art. Streams are launched through the
Sonnenschein Client app itself.
"""

# Import surface kept minimal on purpose: everything beyond os/json is
# imported lazily inside the functions that need it, so an exotic Python
# environment can never kill the whole backend at import time.
import json
import os

try:
    import decky  # type: ignore
except ModuleNotFoundError:
    # Older Decky Loader releases expose the module as decky_plugin —
    # same constants and logger.
    import decky_plugin as decky  # type: ignore

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
    """Parse the Sonnenschein Client QSettings INI: identity + paired hosts.

    Real on-disk layout (verified): identity keys live in [General]; paired
    hosts live in their own [hosts] section as `<idx>\\<field>=<value>`
    (fields: hostname, uuid, localaddress/localport, manualaddress/
    manualport, remoteaddress/remoteport, ...) plus a `size` key.
    """
    conf = {"certificate": None, "key": None, "uniqueid": None, "hosts": []}
    if not os.path.exists(CLIENT_CONF):
        return conf

    section = ""
    hosts = {}
    with open(CLIENT_CONF, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if line.startswith("[") and line.endswith("]"):
                section = line[1:-1]
                continue
            if "=" not in line:
                continue
            key, _, raw = line.partition("=")
            key = key.strip()
            raw = raw.strip()

            if section == "General":
                if key in ("certificate", "key"):
                    val = _unescape_qsettings(raw)
                    if val.startswith("@ByteArray("):
                        val = val[len("@ByteArray(") : -1]
                    conf[key] = val
                elif key == "uniqueid":
                    conf["uniqueid"] = _unescape_qsettings(raw)
            elif section == "hosts" and "\\" in key:
                idx, _, field = key.partition("\\")
                hosts.setdefault(idx, {})[field] = _unescape_qsettings(raw)

    for idx in sorted(hosts, key=lambda i: int(i) if i.isdigit() else 0):
        h = hosts[idx]
        # Prefer a manual address, then LAN, then WAN — same order the
        # client uses for local streaming.
        address, port = "", 0
        for addr_field, port_field in (
            ("manualaddress", "manualport"),
            ("localaddress", "localport"),
            ("remoteaddress", "remoteport"),
        ):
            if h.get(addr_field):
                address = h[addr_field]
                try:
                    port = int(h.get(port_field) or 0)
                except ValueError:
                    port = 0
                break
        if not address:
            continue
        conf["hosts"].append(
            {
                "name": h.get("hostname", address),
                "uuid": h.get("uuid", ""),
                "address": address,
                # NB: this is the HTTP port (47989 default); the HTTPS port
                # is resolved per request via /serverinfo (fallback -5).
                "port": port or 47989,
            }
        )
    return conf



def _xml_unescape(text):
    # &amp; MUST be last
    for entity, char in (("&lt;", "<"), ("&gt;", ">"), ("&quot;", '"'), ("&apos;", "'"), ("&amp;", "&")):
        text = text.replace(entity, char)
    return text


def _check_host_status(text):
    """Raise a clear error when the host answers with an error status_code
    attribute (Moonlight hosts report errors inside an HTTP-200 body)."""
    import re

    m = re.search(r'status_code="(\d+)"', text)
    if m and m.group(1) != "200":
        mm = re.search(r'status_message="([^"]*)"', text)
        msg = _xml_unescape(mm.group(1)) if mm else f"Host-Fehler {m.group(1)}"
        raise RuntimeError(f"Host lehnt ab ({m.group(1)}): {msg} — ggf. Client-App neu pairen.")


def _https_port(address, http_port):
    """Resolve the host's HTTPS port via unauthenticated HTTP /serverinfo
    (same as the client does); fall back to the Sunshine convention -5."""
    import http.client
    import re

    try:
        conn = http.client.HTTPConnection(address, int(http_port), timeout=5)
        try:
            conn.request("GET", "/serverinfo", headers={"User-Agent": "sonnenschein-decky"})
            resp = conn.getresponse()
            body = resp.read()
        finally:
            conn.close()
        if resp.status == 200:
            # NB: no xml.etree here — Decky's frozen Python doesn't bundle it
            m = re.search(r"<HttpsPort>(\d+)</HttpsPort>", body.decode("utf-8", "replace"))
            if m and int(m.group(1)) > 0:
                return int(m.group(1))
    except Exception as e:
        decky.logger.info(f"serverinfo probe failed for {address}:{http_port}: {e}")
    return int(http_port) - 5


class _HostApi:
    """Minimal mutual-TLS client for the Moonlight HTTPS API."""

    def __init__(self, conf):
        self._conf = conf
        self._ctx = None
        self._tmpdir = None

    def _context(self):
        import ssl
        import tempfile

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
        import http.client

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

    async def ping(self):
        """Cheap liveness probe so the frontend can tell a dead backend
        apart from host/network problems."""
        return True

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
        client = _find_client_app()
        return {
            "clientConfFound": os.path.exists(CLIENT_CONF),
            "paired": bool(conf["certificate"]) and bool(conf["hosts"]),
            "clientAppFound": bool(client),
            "clientApp": client,
            "appImagesFound": _list_appimages(),
            "runnerPath": RUNNER,
            "hosts": conf["hosts"],
        }

    async def set_client_path(self, path):
        """Persist a user-picked client path (from the panel file picker)."""
        settings = _read_settings()
        settings["client_path"] = path
        _write_settings(settings)
        return os.path.isfile(path)

    async def get_apps(self, address, port):
        """Fetch the host's app list (unified library) via mutual TLS.

        `port` is the stored HTTP port; the HTTPS port is resolved live.
        """
        conf = _read_client_conf()
        api = _HostApi(conf)
        uid = conf["uniqueid"] or "0123456789ABCDEF"
        import re
        import urllib.parse
        import uuid as uuidlib

        path = "/applist?uniqueid={}&uuid={}".format(
            urllib.parse.quote(uid), uuidlib.uuid4().hex
        )
        body = api.get(address, _https_port(address, port), path)

        # Parsed with re instead of xml.etree: Decky's frozen Python does
        # not bundle xml.etree (proven ModuleNotFoundError on the Deck).
        text = body.decode("utf-8", "replace")
        _check_host_status(text)

        apps = []
        for block in re.findall(r"<App>(.*?)</App>", text, re.S):
            t = re.search(r"<AppTitle>([^<]*)</AppTitle>", block)
            i = re.search(r"<ID>([^<]*)</ID>", block)
            if t and i:
                apps.append({"id": i.group(1).strip(), "title": _xml_unescape(t.group(1).strip())})
        return apps

    async def get_boxart(self, address, port, app_id):
        """Return base64 box art for an app, or empty string if unavailable."""
        import base64
        import urllib.parse
        import uuid as uuidlib

        conf = _read_client_conf()
        api = _HostApi(conf)
        uid = conf["uniqueid"] or "0123456789ABCDEF"
        path = "/appasset?uniqueid={}&uuid={}&appid={}&AssetType=2&AssetIdx=0".format(
            urllib.parse.quote(uid), uuidlib.uuid4().hex, urllib.parse.quote(str(app_id))
        )
        try:
            body = api.get(address, _https_port(address, port), path)
        except Exception as e:
            decky.logger.info(f"No box art for app {app_id}: {e}")
            return {"data": "", "ext": ""}
        ext = "png" if body[:8] == b"\x89PNG\r\n\x1a\n" else "jpg"
        return {"data": base64.b64encode(body).decode("ascii"), "ext": ext}

    async def get_launch_options(self, address, app_title):
        """Launch options string encoding host + app (and the resolved
        client path, so the runner never has to guess) for the runner."""
        host = address.replace("'", "")
        app = app_title.replace("'", "")
        client = _find_client_app().replace("'", "")
        prefix = f"SONNENSCHEIN_CLIENT='{client}' " if client and client != "flatpak" else ""
        return f"{prefix}SONNENSCHEIN_HOST='{host}' SONNENSCHEIN_APP='{app}' %command%"


SETTINGS_FILE = os.path.join(decky.DECKY_PLUGIN_SETTINGS_DIR, "settings.json")

CLIENT_SEARCH_DIRS = (
    "Applications", "applications", "Downloads", "Desktop", ".local/bin", ""
)


def _read_settings():
    try:
        with open(SETTINGS_FILE, "r") as f:
            return json.load(f)
    except Exception:
        return {}


def _write_settings(settings):
    os.makedirs(os.path.dirname(SETTINGS_FILE), exist_ok=True)
    with open(SETTINGS_FILE, "w") as f:
        json.dump(settings, f)


def _find_client_app():
    """Locate the Sonnenschein Client on the Deck.

    Order: user-picked path (settings) → case-insensitive search for a
    Sonnenschein AppImage across common dirs → flatpak.
    """
    override = _read_settings().get("client_path", "")
    if override and os.path.isfile(override):
        return override

    home = decky.DECKY_USER_HOME
    candidates = []
    for sub in CLIENT_SEARCH_DIRS:
        d = os.path.join(home, sub) if sub else home
        try:
            for name in os.listdir(d):
                low = name.lower()
                if low.endswith(".appimage") and "sonnenschein" in low:
                    candidates.append(os.path.join(d, name))
        except OSError:
            continue
    if candidates:
        # Newest by mtime — usually the latest downloaded version
        candidates.sort(key=lambda c: os.path.getmtime(c))
        return candidates[-1]

    import glob
    for flatpak in ("/var/lib/flatpak", os.path.join(home, ".local/share/flatpak")):
        if glob.glob(os.path.join(flatpak, "app", "io.github.elias02345.Sonnenschein*")):
            return "flatpak"
    return ""


def _list_appimages():
    """All AppImages in the search dirs — shown in the panel when the
    client is not auto-detected, so the user sees what IS there."""
    home = decky.DECKY_USER_HOME
    found = []
    for sub in CLIENT_SEARCH_DIRS:
        d = os.path.join(home, sub) if sub else home
        try:
            for name in os.listdir(d):
                if name.lower().endswith(".appimage"):
                    found.append(os.path.join(d, name))
        except OSError:
            continue
    return found


decky.logger.info("Sonnenschein backend module imported OK")
