# Interface

The MyGo front end (`cmd/windows-arm64-apk`, `internal/gui`), built from the screens in `design/`.
It keeps the library and its own settings; running an app is the runtime's, reached only through
package `internal/winapk`. By default it connects to `winapk.Runtime`; `WINAPK_DEMO=1` runs against
`winapk.Mock`, which shows sample apps and says so.

## Windows

| Window | What it holds |
|---|---|
| Main window | The sidebar on the window's base and the current group's panes beside it, floating 8 points from the edges; no title-bar strip: the top-right pane's header draws minimize, maximize and close, and empty header space moves the window |
| An app's window of its own | One app, no sidebar, never split; a header with Back, Rotate, Back to the main window, ⋯ and the window's buttons; closing it stops the app |

- **Sidebar.** Toggle the sidebar (`sb-hide`: a single click collapses it to a 56-point icon rail
  or expands it again; a double-click hides it completely) and Add APK…; Home with the number of
  apps; Open, with one entry per group (a pane alone as a row, several as a card titled with their
  names); In windows of their own; Settings; the runtime's state (Ready to run apps / Can't run apps
  yet), which opens Diagnostics.
- **Tabs.** A pane shows Home, Diagnostics or an open app. Home is never closed; it may be split
  with apps and moved into their group.
- **Groups.** A group is a tree of splits (`layout.go`). Split right / Split down offer the panes of
  other groups, Home and Open an app…; a split is offered only where both halves keep 360 × 280
  points and the group shows fewer than four panes, else the button is greyed out with No room
  beside it / below it. Maximize shows one pane over its group while the others keep running. A pane
  narrower than 560 points folds the window and split buttons into ⋯. An empty group goes away and
  the main window falls back to Home's group.
- **An app's pane.** A phone-style frame scaled into the pane (Phone 412 × 915, Tablet 1024 × 640,
  or Fit the pane without a frame), and a bar with Back (also Esc for the pane in focus), Rotate and
  the screen size. The surface shows the app's state: starting, running,
  not responding (after Stop gets no answer for 5 seconds, with Force stop), stopped (Open again)
  or couldn't start (plain words, Try again, Show details, and Try it with native code off for an
  app run Restricted).
- **Dragging panes.** A pane's header (its icon and name) and a sidebar row drag. Over another
  pane, zones show where it would go: beside it on each side with room (On the left, On the right,
  Above, Below) or in its place (Put it here; the pane it replaces becomes a group of its own);
  within its own group it only trades places (Swap places). Dropped on the sidebar's list it
  leaves its group for one of its own. An app's pane let go outside the main window opens in a
  window of its own; let go inside on no zone, or with Escape, it stays. A window's app dragged
  from the sidebar onto a pane comes back as that pane.
- **Opening.** Each app opens as a pane or in a window of its own by default (its details; new
  apps follow Settings › General). Open uses the default; ▾ offers Open as a pane, Open beside the
  current pane and Open in a window of its own just this once. An app already open comes forward
  where it is. Moving between a pane and a window asks the runtime to re-attach the surface
  (`Process.Reattach`) and restarts the app where it can't.
- **Its screen.** The app is told its screen's size when it opens and whenever it changes:
  rotating, a new screen size, or the room of Fit the pane and of its window. A window of its own
  opens at the app's screen size and turns with it when rotated.
- **Adding and removing APKs.** Add APK… (Ctrl+O), dropping an `.apk` anywhere on the main window,
  or naming one on the command line or to a second launch opens Add an app; nothing registers
  Windows-ARM64.apk for `.apk` files. Adding an app copies its APK into
  `<AppData>\apks\<package>.apk` so deleting or moving the original file does not break future
  launches (and existing library entries are copied there automatically when readable). Removing an
  app lets you either keep its saved data (deleting only the copied APK so reinstalling later
  restores it) or delete both the APK and its app data and converted code; the original APK outside
  Windows-ARM64.apk is never deleted. An app whose stored APK is missing shows File not found and
  Locate…, which reads and copies the file chosen while keeping the app's settings.
- **Keys.** Ctrl+O adds an APK, Ctrl+W closes the pane in focus (Home stays), Ctrl+, opens
  Settings, Esc is Back for the app in focus. The main window's title names the pane in focus.
- **Closing the main window** stops the apps in panes. With Ask first on, a dialog lists which
  stop and which keep running, with Don't ask again. Apps in windows of their own keep running
  unless General says Stop every app.

## Sheets and dialogs

| Sheet | Contents |
|---|---|
| Add an app | `Inspect`'s result: name, version, package, size, the verdict in words, what's inside, how it opens; Add to library or Add and open. Says nothing in the file ran |
| App details | What works here, the last failure, Opens, Screen size, Remove app…; Advanced: native code Off or Restricted with the profile (Use the default, N0, N1, N2), Graphics |
| Settings | General (how new apps open, closing the main window, Ask first, App data with Open folder and Move…, language, appearance); Runtime (which Java runtime, Update / Reinstall, Keep it up to date, default graphics, default native profile); About (Updates: Check for updates; Install X & restart stops every open app so `bin\` is unlocked, shows MB download progress and Installing…, installs the Ed25519-signed update and restarts without the Ask-first confirmation; failures leave the current version intact and are logged) |
| Getting the Java runtime | The card above the panes while none is found, and the step-by-step dialog (download, check, install, test) with Hide and Cancel |
| Confirmations | Closing the main window with apps in panes; removing an app (keep app data and remove only the APK, or delete both) |

Diagnostics is a pane: This PC (the `doctor` report, with the demo banner on sample data) and App
logs: each app's last ten runs since Windows-ARM64.apk started (when, and how each ended), the
chosen run's log with its error category, Errors and warnings only, Copy log and Save for a bug
report…. A run's log stays after its pane closes. Only Diagnostics shows error categories, paths
and versions; panes and Home say what happened in words.

## Files

| File | Holds |
|---|---|
| `%APPDATA%\Windows-ARM64.apk\gui.json` (`demo\gui.json` under `WINAPK_DEMO=1`) | The interface's settings (`Prefs`): theme, language, how new apps open, closing, App data folder, Java folder, Keep it up to date, default graphics and profile, sidebar compact/hidden state |
| `library.json` (same folder) | The library (`entry`): each APK's `Inspect` result and the person's choices for it; not written while the demo's sample library shows |

Both are written atomically (a temporary file renamed over the old one); a missing or broken
file leaves the defaults. The `demo` folder keeps the sample run apart from a real one.

## The runtime ↔ front-end contract

`internal/winapk` (`Runtime`) implements the runtime ↔ front-end contract over `winapk.exe` and
one per-APK runtime process (`winapk run --embed <token>`).

| Call | Runtime contract |
|---|---|
| `Doctor(ctx, javaHome)` | `winapk doctor --json`: `Report` with the Java runtime found (own or chosen, native ARM64 or not), the machine, GPU/Vulkan/ANGLE inventory, Mac.apk commit, and each feature's level (`works`, `experimental`, `unavailable`) |
| `Inspect(ctx, path)` | `winapk inspect --json`: `APK` with package, label (`resources.arsc`), version, size, SDK levels, permissions, ABIs, native libraries, GLES/Vulkan use, the least profile needed (`N0`–`N2`), the verdict (`can-run`, `needs-native`, `not-yet`, `unsupported`) and the launcher PNG icon; runs nothing |
| `Launch(spec, report)` | Starts `winapk run --embed <token>` with a current-user overlapped duplex pipe (`\\.\pipe\winapk-<token>`) and double-buffered top-down `RGBA8` shared memory (`Local\winapk-frame-<token>`); streams `State` (`starting`, `running`, `stopped`, `failed` with a `PLAN.md` §14.2 category, graphics backend, structured log entries) and `*image.RGBA` frames; bounds each `LogsDir` file to 4 MiB and prunes archives to 10 files / 20 MiB |
| `Process.Stop`, `Kill`, `Back`, `Resize`, input | Sends graceful `stop` (`WM_CLOSE`), immediate `Kill`, `back`, `resize` in DIPs, and `pointer`, `wheel`, `key`, `text` over the duplex pipe without coupling Win32 thread input queues |
| `Process.Reattach(window)` | Returns `true`: the front end moves the `*ui.Bitmap` surface between a pane and a standalone window without restarting the runtime process |
| `InstallJava(ctx, dir, progress)` / `CheckJavaUpdate` | Downloads Microsoft Build of OpenJDK 21 ARM64 over HTTPS, verifies its SHA-256 and `0xAA64` PE `jvm.dll`, extracts into `dir`, runs `winapk doctor` before swapping in, and checks for newer `21.x` digests when idle |
| Storage helpers | `ManagedAPKPath`, `CopyAPK`, `RemoveManagedAPK`, `AppDataSize`, `TotalAppDataSize`, `ConvertedCodeSize`, `ClearAppData`, `ClearAppArtifacts`, `ClearConvertedCode` and `MoveDataDir` manage `<AppData>\android`, `apks`, `code` and `logs` |

## Sample data

`winapk.Mock` stands in for the runtime: the design's six sample apps, a sample `doctor` report
marked `Demo`, processes that only report states (Tile Puzzle run Restricted fails with
`GUEST_SYMBOL_UNRESOLVED`; Vulkan Cube and Old Calc fail as not yet / unsupported), and a Java
install that only steps through its progress. Nothing is read from an APK and nothing runs; the
surface and Diagnostics say so. `WINAPK_FRESH=1` starts with an empty library and no Java runtime.
