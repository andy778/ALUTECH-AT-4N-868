# Security Policy

This is a hobby hardware-reverse-engineering project for the ALUTECH AT-4N-868
garage/gate remote. The goal is to understand the RF protocol and rolling-code
scheme on the maintainer's own hardware, not to build a tool for defeating
other people's locks.

## Scope and boundary

This repo documents and helps with:

- Decoding and validating AT-4N-868 frames (CRC, frame layout) from RF captures.
- Dumping firmware from **your own** PY32-based transmitter over SWD.
- Generic, publicly documented crypto tooling (e.g. using `openssl` to decrypt
  a keystore file, given a key you already hold).
- Behavioral analysis of the rolling-code scheme (resync behavior, replay
  windows) for defensive/educational purposes.

It intentionally does **not** include, and contributions should not add:

- The six per-manufacturer "magic" constants (the shared secret keying the
  Feistel cipher) or any means of extracting Flipper Zero's secure-enclave AES
  key. Either would let someone clone arbitrary AT-4N remotes, not just their
  own.
- Tooling aimed at mass scanning, jamming, or opening doors/gates that are not
  yours.

If you are entitled to the manufacturer constants (e.g. recovered them from
your own hardware), that is out of scope for this repo to host or discuss in
detail beyond the generic decrypt path already documented in the README.

## Reporting a concern

If you find something here that goes beyond this scope, or a real
vulnerability that could affect other AT-4N-868 owners (not just "the rolling
code isn't AES-256", which is a known, accepted limitation of this device
class), please open a GitHub issue or contact the maintainer directly rather
than posting exploit details publicly.

This project does not accept vulnerability reports for third-party products
(e.g. Flipper Zero itself) — report those to the respective vendor.
