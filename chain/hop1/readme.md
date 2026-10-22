# hop1 — TDS Gate

This directory intentionally contains no files.
Hop1 is a pure HTTP 302 redirect server (see /redirect_server.py at project root).
All responses are generated programmatically:
- /gate with valid referer  → 302 to hop2 /track
- /gate without referer     → 302 to /decoy (apple pie)
- /decoy                    → 200 apple pie page (served from inline bytes)