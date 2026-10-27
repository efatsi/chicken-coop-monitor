"""MCP server for the chicken coop door.

Reads coop_home's Arduino Cloud variables (see coop_home/thingProperties.h)
and exposes them as tools an agent can call.

Environment:
  ARDUINO_CLIENT_ID, ARDUINO_CLIENT_SECRET  an API key from Arduino Cloud
  COOP_THING_ID                             the coop_home Thing's ID
"""

import json
import os
import time
import urllib.parse
import urllib.request
from datetime import datetime, timedelta, timezone

from mcp.server.mcpserver import MCPServer

API = "https://api2.arduino.cc/iot"
TIMEOUT_SECONDS = 10

mcp = MCPServer("chicken-coop")

_token = {"value": None, "expires_at": 0.0}


def access_token() -> str:
    if _token["value"] and time.time() < _token["expires_at"] - 60:
        return _token["value"]

    form = urllib.parse.urlencode({
        "grant_type": "client_credentials",
        "client_id": os.environ["ARDUINO_CLIENT_ID"],
        "client_secret": os.environ["ARDUINO_CLIENT_SECRET"],
        "audience": API,
    }).encode()
    with urllib.request.urlopen(f"{API}/v1/clients/token", data=form, timeout=TIMEOUT_SECONDS) as resp:
        body = json.load(resp)
    _token.update(value=body["access_token"], expires_at=time.time() + body["expires_in"])
    return _token["value"]


def request(url: str, body: dict | None = None):
    data = json.dumps(body).encode() if body is not None else None
    req = urllib.request.Request(url, data=data, headers={
        "Authorization": f"Bearer {access_token()}",
        "Content-Type": "application/json",
    })
    with urllib.request.urlopen(req, timeout=TIMEOUT_SECONDS) as resp:
        return json.load(resp)


def get(path: str):
    return request(f"{API}/v2/things/{os.environ['COOP_THING_ID']}{path}")


def properties() -> dict:
    return {p["variable_name"]: p for p in get("/properties")}


def rfc3339(t: datetime) -> str:
    return t.strftime("%Y-%m-%dT%H:%M:%SZ")


def raw_series(property_id: str, start: datetime, end: datetime) -> dict:
    # The timeseries endpoint only accepts numeric variables, so read the
    # boolean doorOpen through the raw series query instead.
    return request(f"{API}/v2/series/batch_query_raw", {
        "requests": [{"q": f"property.{property_id}", "from": rfc3339(start), "to": rfc3339(end),
                      "sort": "DESC", "series_limit": 1000}],
        "resp_version": 1,
    })["responses"][0]


@mcp.tool()
def get_coop_door_status() -> dict:
    """Get whether the chicken coop door is open or closed right now.

    Returns `door` as "open", "closed", or "unknown". Trust it only when
    `stale` is false: stale means the coop sensor hasn't checked in for over
    16 minutes (dead battery, out of range), so the last known state may be
    wrong. If the home unit stops reporting to Arduino Cloud, `stale` is also
    set (based on `home_unit_last_reported_at`). Never guess the door state
    when it is "unknown" or stale.
    """
    try:
        props = properties()
    except Exception as e:
        return {"door": "unknown", "stale": True, "error": f"Can't read Arduino Cloud: {e}"}

    door, stale, minutes = props.get("doorOpen"), props.get("remoteStale"), props.get("minutesSinceHeard")
    if door is None or door.get("last_value") is None:
        return {"door": "unknown", "stale": True, "error": "doorOpen has never been reported"}

    # minutesSinceHeard updates every minute while coop_home is online, so its
    # timestamp tells us whether the home unit itself is still alive.
    home_reported_at = minutes.get("value_updated_at") if minutes else None
    home_offline = True
    if home_reported_at:
        reported = datetime.fromisoformat(home_reported_at.replace("Z", "+00:00"))
        home_offline = datetime.now(timezone.utc) - reported > timedelta(minutes=5)

    return {
        "door": "open" if door["last_value"] else "closed",
        "stale": bool(stale and stale.get("last_value")) or home_offline,
        "minutes_since_coop_checked_in": minutes.get("last_value") if minutes else None,
        "door_changed_at": door.get("value_updated_at"),
        "home_unit_last_reported_at": home_reported_at,
    }


@mcp.tool()
def get_coop_door_history(hours: float = 24) -> dict:
    """List door open/close events from the last `hours` hours, newest first.

    Use this for questions like "did the door close last night?" or "when was
    the door last opened?". Times are UTC.
    """
    try:
        door = properties()["doorOpen"]
        now = datetime.now(timezone.utc)
        series = raw_series(door["id"], now - timedelta(hours=hours), now)
    except Exception as e:
        return {"events": [], "error": f"Can't read Arduino Cloud: {e}"}

    return {"events": [
        {"door": "open" if value else "closed", "at": at}
        for at, value in zip(series.get("times", []), series.get("values", []))
    ]}


if __name__ == "__main__":
    mcp.run()
