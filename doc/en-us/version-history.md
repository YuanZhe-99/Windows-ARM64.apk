# Version history

## 0.1.0 (in progress)

- Repository scaffold: working conventions (`AGENTS.md`), the plan (`PLAN.md`), mirrored
  documentation, the documentation mirror check and the Go module.
- Go 1.27.3, MyGo v0.7.4, Mac.apk `main` at `367fd1a` (checked 2026-11-03), imported unchanged as the `macapk/` subtree.
- 2026-11-04: Mac.apk `main` merged at `8b45e48` (upstream's v1.0.2377 sync); Go 1.27.3 and MyGo v0.7.4 checked, unchanged.
- 2026-11-05: development stopped after G2; 0.1.0 was never released or tagged ([README.md](README.md#project-status)).
- 2026-11-16: development resumed; moved to Go 1.27.4, MyGo v0.9.2 and Mac.apk `main` at `3f35611` (upstream's v1.0.2379 sync), and re-verified G0–G2 ([platform-notes.md](platform-notes.md)).
- 2026-11-16: backend P3 thread foundation started (T08); Go 1.27.4, MyGo v0.9.2 and Mac.apk `3f35611` checked, unchanged; version remains 0.1.0, unreleased.
- 2026-11-16: added P3 process-private mutex/condition synchronization (T08 `sync`); Go 1.27.4, MyGo v0.9.2 and Mac.apk `3f35611` checked, unchanged; no new gate or release.
- 2026-11-16: added P3 transactional module graphs and guest dl APIs (T07); full ARM64 regression passes (Host 22, Java 43, Native 58, Bionic 56). Go 1.27.4, MyGo v0.9.2 and Mac.apk `3f35611` checked, unchanged; G3 remains NOT_RUN, no release.
- 2026-11-16: added P3 private-heap allocation and byte/string wrappers (T08 memory); full ARM64 regression passes (Host 22, Java 43, Native 58, Bionic 67). Go 1.27.4, MyGo v0.9.2 and Mac.apk `3f35611` checked, unchanged; no new gate or release.
- 2026-11-16: added P3 mapped regular-file and time APIs (T08 io); full ARM64 regression passes (Host 22, Java 43, Native 58, Bionic 75). Go 1.27.4, MyGo v0.9.2 and Mac.apk `3f35611` checked, unchanged; no new gate or release.
- 2026-11-16: added P3 executable-range guest fault diagnostics (T08 faults); full ARM64 regression passes (Host 22, Java 43, Native 58, Bionic 83). Go 1.27.4, MyGo v0.9.2 and Mac.apk `3f35611` checked, unchanged; mixed JVM/guest faults remain P4, no new gate or release.
- 2026-11-16: added the guest-ABI restricted formatter and fixed host byte-log entry (T08 format); full ARM64 regression passes (Host 22, Java 43, Native 58, Bionic 92). Go 1.27.4, MyGo v0.9.2 and Mac.apk `3f35611` checked, unchanged; complete N1/APK/JNI admission remains unfinished, no release.
- 2026-11-17: completed the declared P3 C/Bionic admission subset with logical system libraries, validated `LIBC` imports and real CRT lifecycle records; normally linked NDK C fixture and full ARM64 regression pass (Host 22, Java 43, Native 58, Bionic 104). Go 1.27.4, MyGo v0.9.2 and Mac.apk `3f35611` checked, unchanged; P4/APK/JNI and G3 remain unfinished, no release.
