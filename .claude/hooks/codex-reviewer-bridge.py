#!/usr/bin/env python3
"""Wake the existing Codex reviewer when Claude writes its inbox.

The shared Markdown file is the instruction. The Codex queue message is only
a notification, so no long-running watcher or temporary Claude courier is needed.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[2]
INBOX = ROOT / ".moai" / "state" / "codex-reviewer-inbox.md"
CONFIG = ROOT / ".moai" / "state" / "codex-reviewer-thread.json"
STATE = ROOT / ".moai" / "state" / "codex-reviewer-dispatch.json"
TITLE = re.compile(r"^# .+?#(\d+)\b", re.MULTILINE)
SENDER = re.compile(r"\*?\*?보낸이\*?\*?\s*:\s*`xpe-leader`")


def matches_inbox(path):
    if not path:
        return False
    candidate = Path(path)
    if not candidate.is_absolute():
        candidate = ROOT / candidate
    return os.path.normcase(str(candidate.resolve())) == os.path.normcase(str(INBOX.resolve()))


def hook_is_relevant(payload):
    if payload.get("tool_name") not in {"Write", "Edit", "MultiEdit"}:
        return False
    tool_input = payload.get("tool_input") or {}
    return matches_inbox(tool_input.get("file_path") or tool_input.get("path"))


def dispatch(dry_run=False):
    if not INBOX.is_file():
        raise RuntimeError(f"reviewer inbox is absent: {INBOX}")
    if not CONFIG.is_file():
        raise RuntimeError(f"reviewer thread config is absent: {CONFIG}")

    data = INBOX.read_bytes()
    content = data.decode("utf-8")
    headings = list(TITLE.finditer(content))
    if not headings or not SENDER.search(content[headings[-1].start() :]):
        raise RuntimeError("reviewer inbox needs a numbered heading and xpe-leader sender")
    instruction = headings[-1].group(1)

    thread = json.loads(CONFIG.read_text(encoding="utf-8")).get("thread_id")
    if not isinstance(thread, str) or not re.fullmatch(r"[0-9a-fA-F-]{36}", thread):
        raise RuntimeError("reviewer thread_id is missing or malformed")

    digest = hashlib.sha256(data).hexdigest()
    if STATE.is_file():
        previous = json.loads(STATE.read_text(encoding="utf-8"))
        if previous.get("sha256") == digest and previous.get("thread_id") == thread:
            return "already queued"

    message = (
        f"xpe-leader posted reviewer instruction #{instruction}. "
        f"Read {INBOX} (SHA-256 {digest}), perform the requested review, "
        f"and write the result to {ROOT / '.moai' / 'state' / 'codex-reviewer-outbox.md'}. "
        "Treat the file as the source of truth; do not reply to a codex-courier session."
    )
    if dry_run:
        return f"would queue instruction #{instruction} for {thread}"

    codex = shutil.which("codex")
    if not codex:
        raise RuntimeError("codex CLI is not on PATH")
    result = subprocess.run(
        [codex, "queue", "--thread", thread, "--message", message],
        cwd=ROOT,
        capture_output=True,
        text=True,
        encoding="utf-8",
        timeout=15,
        check=False,
    )
    if result.returncode != 0:
        raise RuntimeError(f"codex queue failed ({result.returncode}): {result.stderr.strip()}")

    temp = STATE.with_suffix(".json.tmp")
    temp.write_text(
        json.dumps({"thread_id": thread, "instruction": int(instruction), "sha256": digest}) + "\n",
        encoding="utf-8",
    )
    os.replace(temp, STATE)
    return result.stdout.strip()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("mode", choices=["hook", "send", "check"])
    args = parser.parse_args()
    try:
        if args.mode == "hook":
            payload = json.load(sys.stdin)
            if not hook_is_relevant(payload):
                return 0
        outcome = dispatch(dry_run=args.mode == "check")
        if args.mode != "hook":
            print(outcome)
        return 0
    except (OSError, UnicodeError, ValueError, RuntimeError, subprocess.TimeoutExpired) as exc:
        print(f"codex reviewer bridge: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
