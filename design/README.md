# Interface design

The screens the MyGo interface is built from, as standalone HTML: open any file in a browser; the
links between screens work. They are a reference for layout, wording and states, not code: the
interface is native MyGo, built from ordinary controls (lists, buttons, split buttons, segmented
controls, radio lists, menus, panes, a sheet, dialogs), and its behavior follows `PLAN.md` §2.4
and §14 and the behavior contract in `AGENTS.md`. The window structure mirrors Kyokai's
(`Kyokai/doc/en-us/gui.md`: main window, sidebar, pane header, groups, pane sizes, a window of its
own). Sample apps (Hello Java, Notes Lite, GL Triangle, Tile Puzzle, Vulkan Cube, Old Calc),
versions, sizes and the Diagnostics figures are illustrations; the GPU name is a real one on
purpose.

| Screen | File |
|---|---|
| Home alone: the apps; the sidebar with a group and an app in a window of its own | [Library.html](Library.html) |
| Two app panes side by side in a group, ⋯ folded menu and Split down open | [LibrarySplit.html](LibrarySplit.html) |
| An app as a pane of its own, full header, dark theme | [LibraryDark.html](LibraryDark.html) |
| Home, empty: drop or choose an APK, what runs today | [LibraryEmpty.html](LibraryEmpty.html) |
| Add an app: what's inside the APK and what this PC can run | [Inspect.html](Inspect.html) |
| Open's menu: as a pane, beside the current pane, in a window of its own | [OpenMenu.html](OpenMenu.html) |
| An app's menu (right-click on Home) | [ContextMenu.html](ContextMenu.html) |
| An app in a window of its own, with Back to the main window | [Standalone.html](Standalone.html) |
| App details and settings, Advanced open with native profiles | [AppDetails.html](AppDetails.html) |
| An app that couldn't start, in its pane | [StartFailed.html](StartFailed.html) |
| Closing the main window with apps open as panes | [CloseConfirm.html](CloseConfirm.html) |
| No Java runtime: get Windows-ARM64.apk's own | [MissingRuntime.html](MissingRuntime.html) |
| Getting the Java runtime: download and install progress | [RuntimeInstall.html](RuntimeInstall.html) |
| Diagnostics as a pane: this PC (the `doctor` report) | [Diagnostics.html](Diagnostics.html) |
| Diagnostics: an app's log and error category | [DiagnosticsLogs.html](DiagnosticsLogs.html) |
| Settings: General, Runtime, About | [SettingsGeneral.html](SettingsGeneral.html), [SettingsRuntime.html](SettingsRuntime.html), [SettingsAbout.html](SettingsAbout.html) |

## Layout

- **The main window**, as Kyokai's: a sidebar on the window's base (Mica) and beside it the
  panes, rounded boxes floating 8 points from the window's edges and from each other. There is
  no title-bar strip: the system's buttons are hidden and the view draws minimize, maximize and
  close after a separator at the end of the top-right pane's header; an empty part of a header
  moves the window.
- **Sidebar.** At the top Hide the sidebar and **Add APK…**; **Home**, with Windows-ARM64.apk's
  mark and how many apps; **Open** with how many are open, then one entry per group in the order
  they opened — a pane alone as a row (the app's icon, name, its state in words: Running,
  Couldn't start), a group of several as a card with its apps' names ("Notes Lite + GL Triangle")
  over its panes' rows — and last, under "In windows of their own", the apps in a window of their
  own, which a click brings forward. Diagnostics, opened, is an entry like an app. At the bottom
  Settings and the runtime's state as a dot and a few words ("Ready to run apps", "Can't run apps
  yet"), which opens Diagnostics.
- **Home** (Kyokai's Home pane) is the library: Apps, with Check this PC and Add APK…, search, All
  or Open, then one row per app — the APK's icon and label, a plain line on what it is and how it
  opens, a capability chip where something is limited, its state, and a split button: Open (Show
  while open) and ▾. An app that can't run here has Details instead of Open. Home's header has
  Split right and Split down (opening an app beside Home), never close or a window of its own.
- **An app's pane.** Each app opened in the main window is a pane of its own and an entry of its
  own. Its 44-point header: the app's icon, name and state ("Running · native code N2 ·
  Vulkan"); then Open in a window of its own, Split right, Split down, Stop, ⋯ and close (×,
  which stops the app); in a group of several also Maximize (the pane alone over its group, the
  others still running) or Restore. The pane in focus in a group has an accent edge. Below the
  header, on a soft background, a phone-style frame around the app's surface and a small bar:
  Back (also Esc), Rotate, screen size. There is no fake Android status or navigation bar.
- **Groups and sizes** follow Kyokai: a pane dropped on an edge of another splits it there and
  joins its group (beside, above or below; at most four shown); dropped in its middle it takes
  that place; dropped on the sidebar's list it leaves the group. A pane is at least 360 × 280
  points; a split is offered only where both halves keep that, else its item is greyed out and
  says why ("No room beside it"). A pane narrower than 560 points folds Open in a window of its
  own, the splits and Maximize into ⋯, each split a submenu of the other open apps and Home, and
  Open an app….
- **A window of its own**: one app, no sidebar, never split; its surface fills it, no frame or
  margin. Its header: icon, name, Back, Rotate, **Back to the main window** (it comes back as a
  pane, an entry of its own), ⋯ and the window's buttons; closing the window stops the app.
  Dragging a pane out of the main window, or its header's Open in a window of its own, makes one.
- **Choosing how an app opens.** Each app has a default — as a pane or in a window of its own —
  set when it's added, in its details, or for new apps in Settings › General. **Open** uses it;
  **▾** offers, just this once, Open as a pane (an entry of its own, marked Default), Open beside
  the current pane (a split with the pane in focus, as Kyokai's "Beside current") and Open in a
  window of its own, then Change how … opens….
- **Moving a running app** between a pane and a window of its own keeps it running wherever the
  runtime can re-attach its surface; the runtime decides per app. Where it can't, the same
  actions read "Reopen as a pane" / "Reopen in a window of its own" and say the app restarts
  (no screen drawn for that). One app is one runtime process wherever it shows.
- **Stop and force stop.** Stop asks the app to close and leaves its pane showing it stopped;
  × closes the pane and stops it; Force stop (⋯ or right-click) ends its process at once. When
  Stop gets no answer within a few seconds, the pane says the app isn't responding and offers
  Force stop.
- **Closing the main window** stops the apps open as panes; apps in windows of their own keep
  running by default (Settings › General can stop every app instead). While apps are open as
  panes, a confirmation lists which stop and which keep running, with Don't ask again
  (Settings › General: Ask first when apps would stop).
- **Settings** is a sheet over the whole window, as in Kyokai and Nextcloud MyGo: search, then
  Preferences › General; Running apps › Runtime and a link to Diagnostics; Windows-ARM64.apk ›
  About. General's App data has Open folder and **Move…** (moving needs every app stopped;
  whether and how a move is done depends on the runtime).
- **Native code.** Off by default. In an app's details › Advanced: Off or Restricted, and under
  Restricted the **Profile** — Use the default, N0 · Self-contained code, N1 · C library and Java
  calls, N2 · Graphics — each with one plain sentence. Settings › Runtime › Advanced sets the
  default profile. Profile names appear only in these advanced settings, the app pane's state
  line and Diagnostics.
- **The Java runtime.** When none is found, Home says so in a card above the panes and offers to
  get Windows-ARM64.apk's own: OpenJDK 21 for ARM64, its download and installed size, the folder
  it goes in (`%LOCALAPPDATA%\Windows-ARM64.apk\Runtime`), and that it doesn't change Java
  elsewhere on the PC, the PATH or the environment; or Use a folder I choose…. Getting it shows
  download, check, install and test as steps with a progress bar. Settings › Runtime: Use —
  Windows-ARM64.apk's own (installed by it) or A folder I choose… — with Update and Reinstall.
  Nothing here says whether reading an APK needs the runtime.
- **Capability honesty.** Plain words for what works ("Can run: Java only", "Native code support
  is experimental and off", "Vulkan for apps isn't available yet"), never error codes, guest
  offsets, hashes or internal paths outside Diagnostics, which shows `doctor`'s fields and the
  stable error categories of `PLAN.md` §14.2. A chosen graphics backend is never switched.
  Process isolation is crash handling, never a sandbox. Add an app says nothing in the file ran.
- **Not designed, on purpose:** a store, update prompts, and any file-association prompt.

## Open questions

- Where Windows-ARM64.apk's own Java runtime comes from (the screens say Microsoft Build of
  OpenJDK 21.0.5), how its download is verified (checksum and signature are drawn), and whether
  Update checks only when asked (the screens assume so: no automatic updates).
- Whether Diagnostics should stay a pane in the Open list (as drawn, like Kyokai's status pane)
  or open as a sheet like Settings.

## Tokens

The same as Nextcloud MyGo and Kyokai.

| Token | Light | Dark |
|---|---|---|
| Window base (Mica) | `#E9EEF3` | `#1A1C21` |
| Pane | `#FFFFFF` | `#23262C` |
| Border | `#DCE2E8` | `#33363E` |
| Text | `#1B1D21` | `#EDEFF2` |
| Secondary text | `#4A5260` | `#A9AFBA` |
| Accent (fills with white text; focus edge) | `#0072B5` | `#4DAEE8` (with dark text) |
| Accent text and links | `#0069A6` | `#7CC6F2` |
| Success | `#0F7B3F` | `#4CC38A` |
| Needs attention, experimental | `#8A4B00` on `#FFF6E5` | `#F0B65A` on `#352812` |
| Danger, can't run | `#B42318` on `#FDEDEB` | `#F2877D` on `#3A1E1C` |

Type is Segoe UI Variable; sizes 12 / 13 / 14 body, 15–16 section titles, 20–24 page titles;
diagnostics values in Cascadia Mono. Radii: 6 controls, 8 panes, cards and menus, 10 dialogs and
the sheet; the phone frame 30 outside, 22 at the screen. Pane headers are 44 points. The window
keeps an 8-point margin around its panes. The product icon drawn here — a window holding a
phone, on the accent — is a placeholder of Windows-ARM64.apk's own; it is never the Mac.apk logo
or the Android robot. App icons are stand-ins for the icons the APKs carry. Inside the frame and
an app's window, the app's own drawing is not bound by these tokens.
