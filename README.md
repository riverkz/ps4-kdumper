Kernel Dumper
===
[![Codacy Badge](https://app.codacy.com/project/badge/Grade/)](https://www.codacy.com/gh/Scene-Collective/ps4-kernel-dumper/dashboard)

## Synopsis
Dumps your device's kernel from memory to a USB device.

For the payloads built with the most recent firmware supported visit [this repo].

## Notes
- Contribute translations for the decription in `meta.json`
- Requires being built with the [Scene Collective Payload SDK]
- Supports any firmware that the SDK supports
- Firmware **14.00** is supported (via the SDK's `K1400_*` offsets). `meta.json` reflects the newest firmware the payload was set up against; older firmware still works
- The kernel base comes from `get_kernel_base()`, which reads the same per-firmware table the SDK uses, so the dumper needs a kernel exploit that already has the console jailbroken. On 14.00 that is [afterglow] — the kernel offsets it needs are in `afterglow-offsets.txt`
- The dump size is read out of the kernel's own ELF header and sanity-checked; if the base or the header looks wrong the payload notifies and quits instead of writing a corrupt `kernel.bin`
- Originally created by [eversion]

[//]: #
  [Scene Collective Payload SDK]: <https://github.com/Scene-Collective/ps4-payload-sdk>
  [this repo]: <https://github.com/Scene-Collective/ps4-payload-repo>
  [eversion]: <https://github.com/eversion/PS4-Kernel-Dumper>
  [afterglow]: <https://github.com/well0nez/afterglow>
