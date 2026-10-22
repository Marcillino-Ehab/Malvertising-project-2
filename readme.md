# Malvertising Lab — Simulated Campaign (Academic Project)

Full malvertising campaign simulation in an isolated local Docker environment.
All payloads are defanged/simulated — nothing leaves localhost, no real
systems are targeted.

## Quick Start (any machine)
1. Install Docker Desktop
2. Build the payloads:
   docker run --rm -v $(pwd)/payload:/src -w /src ubuntu:24.04 bash -c \
     "apt update -qq && apt install -y -qq g++ libcurl4-openssl-dev >/dev/null && \
      g++ -O2 -std=c++17 dropper.cpp -o dropper -lcurl && \
      g++ -O2 -std=c++17 stage2.cpp -o stage2 -lcurl && \
      cp dropper sysupdate.bin"
3. docker compose up -d
4. Open http://localhost:8088
5. Click an ad — watch the full campaign chain (DevTools → Network tab)

No configuration needed — everything runs on localhost.

## Attack chain
Publisher site → malicious ad (fingerprinting + cloaking)
→ TDS gate hop1 (referer check, HTTP 302) → tracking hop2 (302)
→ scareware scan hop3 → drive-by download (hidden iframe)
→ fake-update page → dropper (stage-2 fetch + persistence)
→ implant (anti-analysis + C2 beaconing)

## Services
| Port | Service |
|------|---------|
| 8088 | Publisher (news site) |
| 8080 | Ad server |
| 8081 | Hop1 — TDS gate |
| 8082 | Hop2 — tracking |
| 8083 | Hop3 — scareware |
| 8084 | Payload host |
| 9090 | C2 listener (beacon receiver) |

## Test cases
- Visit http://localhost:8081/gate with NO referer → 302 to /decoy (apple pie)
- Click the ad from the publisher → full 302 chain
- Run dropper inside a test container → stage2 dropped + persistence installed
- Run stage2 → beacons visible in `docker logs -f c2-listener`
- Run stage2 as user "sandbox" or under gdb → exits silently (anti-analysis)

## Cleanup
docker compose down