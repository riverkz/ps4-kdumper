# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.1.0] - 2026-11-11

### Added
- Support for firmware 14.00 (relies on the `K1400_*` offsets and the `build_kpayload` case in the Scene Collective Payload SDK); the kernel offsets afterglow needs on 14.00 are recorded in `afterglow-offsets.txt`
- Explicit firmware / kernel base sanity checks: the payload now notifies and quits instead of writing a corrupt dump when the SDK cannot resolve the running firmware
- ELF header validation in `get_kernel_size` (header, entry and segment count bounds, power-of-two segment alignment) and a hard cap on the resulting dump size, so a bad kernel base cannot turn into a multi-gigabyte `kernel.bin`

### Changed
- Enlarged `fw_version` buffer to 8 bytes, so 4-digit firmware strings (`xx.xx`) and any future `xxx.xx` have room with the terminator
- Replaced the hardcoded kernel virtual address with a named `KERNEL_BASE_VADDR` constant and documented that it is identical on every firmware, including 14.00
- CI: `sanity-check.yml` moved off the deprecated `actions/checkout@v2` / `actions/upload-artifact@v2` (GitHub now hard-fails runs that use them, before the first step), pins `PS4SDK` at job level so `make` actually finds `libPS4`, and names the two artifacts instead of relying on the default `artifact` name for both

##  [1.0.0] - 2021-01-01

### Added
- Using the semantic versioning via `CHANGELOG.md`
- `.gitignore` setup to ignore bin and map files as well as the build directory
- Workflow file for pull requests for sanity testing

### Changed
- Initial commit to support offloading all firmware specific offsets to the SDK

### Removed
- Commit history. Starting versioning with a clean slate.

[unreleased]: https://github.com/Scene-Collective/ps4-kernel-dumper/compare/v1.1.0...HEAD
[1.1.0]: https://github.com/Scene-Collective/ps4-kernel-dumper/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/Scene-Collective/ps4-kernel-dumper/releases/tag/v1.0.0
