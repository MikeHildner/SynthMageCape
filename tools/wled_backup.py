"""Snapshot the WLED controller's settings into wled/ so git can show what changed.

WLED is stock firmware (a release binary, no source changes), so the only
things that change on the controller are two JSON files:

    cfg.json      hardware and settings (pins, LED counts, buttons, usermods)
    presets.json  saved presets

WLED stores both minified on one line, which makes diffs useless, so this
script re-indents them. WLED accepts the indented files on restore
(Config -> Security & Updates -> Restore).

cfg.json holds the Wi-Fi network name and the length of each password, but
not the passwords themselves (those live in a separate file WLED does not
serve).

Usage:  python tools/wled_backup.py [host]      default host 192.168.1.34
"""
import json
import sys
import urllib.request
from pathlib import Path

HOST = sys.argv[1] if len(sys.argv) > 1 else "192.168.1.34"
OUT = Path(__file__).resolve().parent.parent / "wled"


def main():
    OUT.mkdir(exist_ok=True)
    for name in ("cfg.json", "presets.json"):
        with urllib.request.urlopen(f"http://{HOST}/{name}", timeout=10) as r:
            data = json.load(r)
        (OUT / name).write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
        print(f"saved wled/{name}")


if __name__ == "__main__":
    main()
