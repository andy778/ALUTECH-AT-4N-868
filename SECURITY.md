# Security Policy

Hobby hardware-RE project for the ALUTECH AT-4N-868 garage/gate remote.
Goal: understand the RF protocol and rolling-code scheme on the
maintainer's own hardware — not to defeat other people's locks.

## Scope

In scope:

- Decoding/validating AT-4N-868 frames (CRC, frame layout) from RF captures.
- Dumping firmware from **your own** PY32-based transmitter over SWD.
- Generic crypto tooling (e.g. `openssl` to decrypt a keystore file, given a
  key you already hold).
- Behavioral analysis of the rolling-code scheme (resync, replay windows),
  defensive/educational.

Out of scope, do not contribute:

- The six per-manufacturer "magic" constants (the Feistel cipher's shared
  secret), or any means of extracting Flipper Zero's secure-enclave AES key.
  Either clones arbitrary AT-4N remotes, not just your own.
- Tooling for mass scanning, jamming, or opening doors/gates that aren't
  yours.

If you're entitled to the manufacturer constants (recovered from your own
hardware), that's still out of scope to host or discuss here beyond the
generic decrypt path already in the README.

## Reporting a concern

Something beyond this scope, or a real vulnerability affecting other
AT-4N-868 owners (not just "the rolling code isn't AES-256" — a known,
accepted limitation of this device class): open a GitHub issue or contact
the maintainer directly, not a public exploit writeup.

No vulnerability reports for third-party products (e.g. Flipper Zero) —
report those to their vendor.
