#!/usr/bin/env bash
#
# Decrypt the Flipper Zero "alutech_at_4n" SubGhz keystore asset.
#
# The keystore ships only the AES-128-CBC ciphertext and its IV; the AES key
# lives in the Flipper's secure enclave and is NOT included here. You must
# supply it yourself. Without the correct key this prints random-looking bytes.
#
# Usage:
#   ./decrypt_keystore.sh <KEY_HEX>
#
#   KEY_HEX = 32 hex characters (16-byte AES-128 key)
#
# On success the 32-byte plaintext contains the six 32-bit constants used by the
# rtl_433 decoder:  -R 321:<m0>,<m1>,<m2>,<m3>,<m4>,<m5>
#
set -euo pipefail

# Values taken verbatim from alutech_at_4n.keystore (public, non-secret).
IV="8864A6A64447678AD63236F6B9065731"
CT="E811BD4F0955D217AE6677906E799D45D8DAAFD1F7923E1660B5E24574631B60"

KEY="${1:-}"
if [[ -z "$KEY" ]]; then
    echo "usage: $0 <KEY_HEX>   (32 hex chars = 16-byte AES-128 key)" >&2
    exit 1
fi
if [[ ! "$KEY" =~ ^[0-9A-Fa-f]{32}$ ]]; then
    echo "error: KEY must be exactly 32 hex characters" >&2
    exit 1
fi

tmp_ct="$(mktemp)"
tmp_pt="$(mktemp)"
trap 'rm -f "$tmp_ct" "$tmp_pt"' EXIT

printf '%s' "$CT" | xxd -r -p > "$tmp_ct"

# -nopad: the keystore stores raw block-aligned data (no PKCS#7 padding).
openssl enc -d -aes-128-cbc -K "$KEY" -iv "$IV" -nopad -in "$tmp_ct" -out "$tmp_pt"

echo "Decrypted plaintext (hex):"
xxd "$tmp_pt"

# First 24 bytes = six 32-bit constants (big-endian, keystore order).
#
# Big-endian is what the Flipper firmware does when it loads the keystore:
#   for (i = 0; i < 4; i++)
#       magic_data = (magic_data << 8) | buffer[i];
# (lib/subghz/protocols/alutech_at_4n.c) -- buffer[0] becomes the most
# significant byte, so each 4-byte entry is a big-endian uint32.
hex="$(xxd -p "$tmp_pt" | tr -d '\n')"
params=""
for i in 0 1 2 3 4 5; do
    word="${hex:$((i * 8)):8}"
    params+="$word"
    [[ $i -lt 5 ]] && params+=","
done

echo
echo "Six 32-bit constants (big-endian, keystore order):"
tr ',' '\n' <<< "$params"

echo
echo "rtl_433 parameter string:"
echo "  -R 321:${params}"

echo
echo "Ready-to-run example:"
echo "  rtl_433 -f 868.35M -s 1024k -R 321:${params}"
