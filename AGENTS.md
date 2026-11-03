# AGENTS.md

Operating guide for agents working on **Windows-ARM64.apk**. This file holds **only** rules about
how to work here. Everything describing what the code *is* or *does* lives in `doc/en-us/` — see
[Where to read what](#where-to-read-what).

Windows-ARM64.apk runs selected Android APKs as native Windows 11 ARM64 processes. It is a port of
[Mac.apk](https://github.com/kksimp/Mac.apk): it reuses Mac.apk's DEX → JVM translation and
Android Framework on a native ARM64 JVM, and adds, step by step, an Android ARM64 ELF loader, a
Bionic/JNI bridge and graphics bridges (GLES/EGL → ANGLE → Vulkan, Android Vulkan → Windows
Vulkan). The product front end is a Go + MyGo application that opens an app either in a
phone-style pane inside its main window or in a standalone window; each APK runs in its own
runtime process. Treat the user's message as the change request: plan, implement, verify, report.

## Reading order

When you need to understand code, read in this order and stop as soon as you have what you need:

1. **`doc/en-us/`** — start here, always. `functions/INDEX.md` to find the right package or
   module; the concept docs for behavior.
2. **`PLAN.md`** — while it is active, for anything not yet implemented: architecture, ABI
   constraints, phases P0–P8, gates G0–G5, fixtures and acceptance.
3. **Comments in the source** — the Function Explanation Layer on each declaration.
4. **The implementation** — only when the docs and comments are insufficient, or when you must
   confirm actual behavior before changing it.

Do not jump straight to reading source bodies. Where docs and code disagree on something you are
about to change, verify against the code, then fix the docs in the same commit.

## Where to read what

| Question | Read |
|---|---|
| Architecture, ABI rules, phases, gates, fixtures, acceptance (while the plan is active) | `PLAN.md` |
| Interface screens, wording and design tokens (reference for the MyGo front end) | `design/` |
| Verified upstream contracts (Mac.apk, Windows ARM64 ABI, Android NDK, MyGo) and quirks | `doc/en-us/upstream.md` |
| Reused third-party code, licenses and obligations | `doc/en-us/third-party.md` |
| What is implemented and verified per platform, with gate status | `doc/en-us/platform-notes.md` |
| Environment, commands, dev loop | `doc/en-us/development.md` |
| English→Chinese terminology | `doc/en-us/translation-guide.md` |
| Past releases and the upstream versions they used | `doc/en-us/version-history.md` |
| Per-package and per-module roles | `doc/en-us/functions/INDEX.md` |

New concept docs (the P0 port audit, ABI compatibility, native profiles, graphics capabilities,
the compatibility report, decisions, the runtime ↔ front-end contract) are added with the code or
results they describe; add their rows here in the same commit.

## Division of work

Three kinds of agents work here, following `PLAN.md` §2.4 and §6:

- The **runtime agent** (the plan's Coding Agent) owns the Mac.apk subtree (`macapk/`) and every
  change to it, the Windows host and guest code inside it, the `winapk` CLI, `scripts/windows/`,
  `tests/windows-arm64/`, `build-lock.json`, packaging and the capability/compatibility reports.
  It starts at P0 and follows the phase order of `PLAN.md` §6.
- The **design agent** designs the front end with Kyokai as its reference: the main window, the
  phone-style app pane, the standalone app window and how a person chooses between them. It
  delivers standalone screens and wording in `design/`; it does not write product code.
- The **frontend agent** owns the MyGo front end: the Go module at the repository root (`cmd/`,
  `internal/`, `locales/`, `resources/`), built from `design/`.
- The front end reaches the runtime only through the runtime ↔ front-end contract (process launch,
  the `winapk` JSON outputs, and the window/Surface hand-off for the pane). The runtime agent
  defines it with the design and records it in `doc/en-us/` before the frontend depends on it; a
  change to it updates both sides and their tests in one change set. Neither side reaches past it,
  and the plan does not prescribe one window-embedding technique.

`PLAN.md` paths are relative to the Mac.apk tree or to the repository; they map as follows:

| `PLAN.md` path | Repository path | Why |
|---|---|---|
| upstream paths (`runtime/…`, `scripts/*.sh`, `debug/…`, `launcher/…`, `thirdparty/…`) | `macapk/…` | Mac.apk is a git subtree under `macapk/`, edited in place |
| new runtime code (`runtime/platform/`, `runtime/guest/`, `cmake/platforms/`) | `macapk/runtime/platform/`, `macapk/runtime/guest/`, `macapk/cmake/platforms/` | it builds with the upstream CMake tree |
| `scripts/windows/`, `tests/windows-arm64/`, `tests/fixtures/` (generated) | same paths at the root | Windows tooling around the tree; commands run from the root |
| `build/windows-arm64/` | same, under the ignored `build/` | shared with `mygo build` output |
| `docs/windows-arm64/*.md` (audit, ABI, profiles, graphics, compatibility) | `doc/en-us/*.md`, mirrored in `doc/zh-cn/` | one documentation tree, mirrored |
| `docs/windows-arm64/adr/` | `doc/en-us/decisions.md` | same |

## Required workflow

1. Treat the user's message as the modification request.
2. Before editing, fetch the remote(s) and check whether the local branch is behind. Resolve any
   divergence before starting.
3. Check the upstreams per [Upstream versions](#upstream-versions).
4. Read per [Reading order](#reading-order). Find the phase and gate (`PLAN.md` §6) the change
   belongs to; do not start work whose dependencies are not done, except where §6 allows it (P2
   may start right after P0).
5. Keep changes scoped. Do not revert unrelated work in the tree.
6. Update documentation in the same change set — see
   [Documentation maintenance](#documentation-maintenance).
7. Verify — see [Verification](#verification).
8. Report briefly, in English and Chinese, with what `PLAN.md` §16 and §20 ask: what changed, the
   commands run, each gate and layer as `PASS / FAIL / BLOCKED / NOT_RUN` with evidence, what is
   still unverified, the next blocking issue, any deviation from the plan, the merged Mac.apk
   commit, the current version and the configured remotes.
9. Ask before pushing. The user decides when to release.

## Documentation maintenance

**Docs are the primary artifact. Update them first, and never let them drift.**

Any change to a behavior, a data format, the runtime ↔ front-end contract, a guest ABI rule, a
native profile, a graphics capability, a change to the upstream tree or a user-visible feature
updates, in the same commit, the affected concept docs and the `functions/INDEX.md` row.

Every language directory under `doc/` (currently `en-us` and `zh-cn`) mirrors the others exactly —
same files, headings, tables, and code blocks. `doc/en-us/` is authoritative: any documentation
change updates **all** language directories in the same commit, translated per
`translation-guide.md`; new terms go into its glossary. `go run ./tools/checkdocs` must pass.

**Put explanation in the docs, not here.** This file is for agent instructions only.

**`PLAN.md` is the active plan** (written in Chinese with the user) and is never copied into
`doc/`. Its design decisions change only when the user decides; record a deviation you need as a
question to the user, or as an ADR in `doc/en-us/decisions.md` where the plan itself asks for one
(§1.9, §4.1, §11.1). When a phase is done, its long-lived contracts (ABI tables, profiles,
capabilities, the loader and bridge designs as built) move into the concept docs, and the plan's
sections for it shrink to a pointer. When the plan is complete, delete `PLAN.md`.

Upstream Mac.apk documents (`macapk/README.md`, `macapk/BUILDING.md`, …) describe the macOS
product and stay as upstream wrote them; Windows-ARM64.apk documentation lives only in `doc/`.

Add a `doc/en-us/version-history.md` entry for each release. Documentation-only commits do not
bump versions or create tags.

## Authoring rules

**Function Explanation Layer.** Every function, method and significant type carries a doc comment
(Go doc comments; `///` on C/C++ declarations in headers; Javadoc on Java) whose first sentence
states its purpose ("Maps …"). Non-trivial declarations add short lines as needed:

- `Inputs:` important parameters only
- `Returns:` what the caller receives
- `Side effects:` state, file, memory-protection, process or UI effects
- `Notes:` assumptions, edge cases, ABI domain (host or guest), the plan rule a check enforces

Keep comments accurate in the same change as the code: they are the third layer of the reading
order, and a stale comment leads to a wrong change. Upstream code keeps its own comment style;
new or substantially rewritten declarations in `macapk/` follow this rule.

**Code guidelines** (shared by the MyApps projects):

- Do not over-scope: make only the changes that are requested or clearly necessary; no speculative
  features, flags or configurability, and no empty packages or directories waiting for later work.
- Do not over-abstract: no interface with a single implementation, no builders for simple objects,
  no helpers for one-time operations; fewer cohesive files beat many thin wrappers; keep
  implementation details private. The platform interfaces of `PLAN.md` §4.3 are the exception
  they name: each has a macOS and a Windows side, and grows only with real callers.
- Do not over-defend: validate at true boundaries — APK and ELF input, guest calls into the host,
  JNI from the JVM, OS and driver callbacks, files read from disk — and trust internal callers.
- Use the current language versions idiomatically: the latest Go (generics where they remove
  duplication); C/C++ and Java at the standards the upstream tree uses.
- Name concisely; reuse existing helpers and conventions; when removing code, remove it completely.

**Upstream code.** Mac.apk lives under `macapk/` as a git subtree and is changed in place. Keep
changes to it small and platform-conditional, never rename, reformat or reorganize upstream code
or its Java packages, and never break the macOS build: Objective-C++, Apple frameworks and
Homebrew paths stay behind macOS conditions. Prefer a platform interface with a new Windows
implementation over editing shared logic. Import and update it only with `git subtree`
(see [Upstream versions](#upstream-versions)).

**Two ABI domains.** Host Windows ARM64 code and guest Android ARM64 code never share types,
`va_list`, CRT objects, pthread objects, handles or unwinding; every crossing goes through an
explicit guest type and a thunk or a signature proven compatible. Guest `long` is 64-bit; never
use a Windows `long` for it.

**Tests.** The product requires tests and fixtures (`PLAN.md` §16), which overrides the MyApps
default of writing tests only on request. Every fixture has source, build parameters, a hash and
expected output; every capability has a positive and a negative case; every fixed bug gets a
regression test. Name the fixture ID (`T01`…) or gate a test covers. No tests for wording or
internal state details. A fixture built with special flags never proves behavior of ordinary NDK
builds.

Other conventions:

- MyGo UI code follows `.agents/skills/mygo-maintenance`: views are functions of state, elements
  are never stored, background results reach the view through `Window.Update`, controls get stable
  keys where state applies.
- Interface copy is written for the person using the app. Never surface error codes, guest
  offsets, hashes or internal paths in the main interface; a diagnostics view may show them.
- Interface strings go through the ARB catalogs; `go tool mygo i18n check` must pass.
- The product name is **Windows-ARM64.apk** everywhere (window, About, installer, shortcuts,
  release material); `winapk` is only the internal CLI and build name. It has its own icon, never
  the Mac.apk name or logo as its identity; it may say "based on Mac.apk".

## Behavior contract

Do not change these without the user explicitly deciding to. `PLAN.md` §1, §5 and §17 bind every
change.

- **Native means native.** Host executables, DLLs and the JVM are ARM64 PE; no ARM64EC, no x64
  components in the package, checked by tooling. Build-time tools may run emulated; that is never
  reported as a native run.
- **No Android system underneath.** No WSA, WSL, Android VM, Android system image or Linux user
  space at run time. The JVM is the design, not an exception.
- **Reuse, don't rewrite.** The DEX translator, Framework and APK/resource parsing come from
  Mac.apk; the front end never implements runtime behavior, and the runtime never grows a second
  Android Framework.
- **The Java path always ships.** Pure-Java APKs build, run, test and release independently of
  guest native, ANGLE, Vulkan and Skia; a failing native gate never blocks it.
- **Host state is sacred.** Never repurpose `x18` or write `TPIDR_EL0`, never pass a Linux `svc` to
  Windows, never use long-lived RWX pages or `LoadLibrary` on an ELF, never pre-empt HotSpot's
  exception handling, and never let guest C++ exceptions cross into host code or the JVM.
- **No fake success.** Unknown relocations, imports, TLS models, extensions and JNI entries fail
  with a stable error category (`PLAN.md` §14.2); no catch-all stub returns 0, no Android extension
  is announced before its bridge works, and a window appearing is not a feature working.
- **Guest native is off by default** and runs only within a declared, tested profile (N0, N1, N2);
  static scanning screens binaries, it never proves arbitrary `.so` files safe.
- **One APK, one runtime process,** opened in the pane or in a standalone window. Process isolation
  is for fault handling; it is never described as a security sandbox.
- **Graphics backends report separately.** GLES and guest Vulkan pass or fail on their own; D3D11
  passing never stands in for Vulkan; an explicit backend choice is never silently switched.
- **Leave the system alone.** Never register as the default APK handler, change global
  environment, copy GPU drivers or install anything silently. The one thing it installs is its own
  private Java runtime, when none is found, in its own folder and with the person seeing it, and
  it may keep that runtime updated (verified like the first download) while that setting is on;
  never the system's Java, `PATH` or environment. The app itself never updates on its own
  (`PLAN.md` §2.4).
- **Never claim more than was verified;** each layer (A Java MVP, B Native PoC, C Graphics PoC,
  D experimental release) is reported on its own, and macOS regressions not run are `NOT_RUN`.

## Upstream versions

**Go, MyGo and Mac.apk are never pinned: they always follow their latest version.** Every change
set checks and moves to:

- the latest Go release (the `go` line in `go.mod`; `https://go.dev/dl/?mode=json`);
- the latest MyGo tag (`go list -m -versions github.com/egoist/mygo`), migrating the UI code per
  `.agents/skills/mygo-maintenance`;
- the latest Mac.apk `main` (`git ls-remote https://github.com/kksimp/Mac.apk main`), merged with
  `git subtree pull --prefix=macapk https://github.com/kksimp/Mac.apk main --squash`. After a merge,
  re-check the upstream entry points of `PLAN.md` §3.1 and `doc/en-us/upstream.md`, update the port
  audit, and re-run the gates that the merged files touch; a release waits until they pass, it does
  not fall back to an older upstream.

The other dependencies (JDK, Android SDK/NDK, ANGLE, Vulkan Loader, Skia, clang-cl, Windows SDK,
CMake, Ninja) are chosen in P0 and locked in `build-lock.json` as `PLAN.md` §7 asks. For Mac.apk,
Go and MyGo the lock file is a record of what a build used, not a pin. Add a `version-history.md`
line naming the versions; record "checked, unchanged" too.

## Verification

```bash
gofmt -l cmd internal tools
go vet ./...
go test ./...
CGO_ENABLED=0 go test ./...
go tool mygo vet .
go tool mygo i18n check
go run ./tools/checkdocs
```

- The runtime build, `doctor` and fixture suites (`scripts/windows/*.ps1`, `PLAN.md` §18) join this
  list, and `development.md`, with the first runtime code. "No tests executed" is never a pass.
- Run on real Windows 11 ARM64 hardware whenever a change touches the JVM, the loader, the ABI,
  JNI or graphics; an x64 machine can only report parsing or cross-compilation results. Graphics
  gates need a desktop session and a GPU.
- Check the machine type of every built and shipped PE, DLL and ELF when a change touches the build
  or the package.
- Inspect the native app (screenshots) when a change touches layout; screenshots never replace
  value, lifecycle and error assertions.
- When a change touches shared Java/DEX code or upstream build files, run the macOS build and smoke
  where a Mac is available, otherwise record them as `NOT_RUN`.
- **Never claim "verified" without an actual run;** record what was verified where, on which
  machine, GPU and driver (`doc/en-us/platform-notes.md`), using the report template of
  `PLAN.md` §16.

## Release, version, commit, tag, push

The version lives in `internal/version` (`Version`); `mygo.json` must match (a test enforces it),
and the `winapk` runtime reports the same version. Front end and runtime are released only
together.

For ordinary work, do not bump versions or tag until the user confirms the release version and
confirms pushing. When the user confirms:

1. Set the version in `internal/version/version.go` and `mygo.json` (and the runtime's version
   source once it exists).
2. Bring Go, MyGo and Mac.apk to their latest versions and re-run verification.
3. Commit, including the `doc/en-us/version-history.md` entry (and its mirror).
4. Create an annotated tag `vX.Y.Z`.
5. Push **the commit first**, then the tag, to every configured remote.

Commits follow the granularity of `PLAN.md` §19: one capability per commit with its test results;
never one "Windows port" commit mixing refactoring, renaming and compatibility work.

Signing keys and certificates live only in CI secrets and the maintainer's offline copy.

## Agent co-author attribution

An agent that made a real, material contribution to a commit may add its own accurate
`Co-authored-by:` trailer. Attribution is per commit: do not add an agent merely because it
reviewed, observed, or continued another agent's work, and never copy a trailer automatically from
an earlier commit. When multiple agents materially contributed, include one accurate trailer each.
Use the agent's actual documented identity; never invent a provider, model, name, or email.
Approved examples are `Co-authored-by: Codex <noreply@openai.com>` and, for Claude Code,
`Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>` with the model name replaced by the actual
model that did the work.

## Remotes and secrets

- `origin` → `<local_gitea_address>` (private Gitea)
- `github` → not configured yet; the user names the public repository when it is created.

Determine the repository path from the runtime workspace; do not hardcode a machine-specific
absolute path here.

**Masking rule:** keep the `origin` URL written as `<local_gitea_address>` in every committed file.
Never write the underlying host or port anywhere in the repo. Treat the repository as public:
committed files never name private repositories, machines, servers, accounts or where keys are
kept. Test APKs come only from sources that may be published; private or commercial APKs and
their extracted contents are never committed, logged or used as fixtures.

**Never commit:** secrets, signing keys or certificates, `.dev/`, `build/`, CMake build trees,
generated fixtures (`tests/fixtures/`), JDK/NDK/ANGLE downloads, app data or caches from test
runs, minidumps, or machine-specific state.

## MyApps milestone releases

For each completed gate (G0–G5), increment the patch version by 0.0.1; the user decides when to
release. Documentation-only changes do not bump the version.

License: AGPL-3.0 (`LICENSE`), as required by Mac.apk's AGPL-3.0 code (Mac.apk is dual-licensed;
this project uses the AGPL). Upstream files keep their own headers; releases ship the license
texts, notices and the complete corresponding source, including the `macapk/` changes. MyGo's MIT
license is recorded separately. "Mac.apk" and its logo belong to the Mac.apk project
(`macapk/TRADEMARKS.md`); this project is not the official Mac.apk.
