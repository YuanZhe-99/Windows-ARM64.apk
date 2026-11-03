# Interface design

The screens the MyGo interface is built from, as standalone HTML: open any file in a browser; the
links between screens work. They are a reference for layout, wording and states, not code: the
interface is native MyGo, built from ordinary controls (lists, buttons, split buttons, segmented
controls, menus, panes, a sheet, dialogs), and its behavior follows `PLAN.md` §2.4 and §14 and the
behavior contract in `AGENTS.md`. Sample apps (Hello Java, Notes Lite, GL Triangle, Vulkan Cube,
Old Calc, Tile Puzzle), versions, sizes and the Diagnostics figures are illustrations.

| Screen | File |
|---|---|
| Library: the app list, with Notes Lite running in the phone pane | [Library.html](Library.html) |
| Library, dark theme, GL Triangle in the pane | [LibraryDark.html](LibraryDark.html) |
| Library, empty: drop or choose an APK, what runs today | [LibraryEmpty.html](LibraryEmpty.html) |
| Add an app: what's inside the APK and what this PC can run | [Inspect.html](Inspect.html) |
| Open's menu: in the pane or in its own window, once | [OpenMenu.html](OpenMenu.html) |
| An app's menu (right-click): show, move, stop, force stop | [ContextMenu.html](ContextMenu.html) |
| An app in its own window, with Move into the pane | [Standalone.html](Standalone.html) |
| App details and per-app settings, Advanced open | [AppDetails.html](AppDetails.html) |
| An app that couldn't start, in the pane | [StartFailed.html](StartFailed.html) |
| Java runtime not found | [MissingRuntime.html](MissingRuntime.html) |
| Diagnostics: this PC (the `doctor` report) | [Diagnostics.html](Diagnostics.html) |
| Diagnostics: an app's log and error category | [DiagnosticsLogs.html](DiagnosticsLogs.html) |
| Settings: General, Runtime, About | [SettingsGeneral.html](SettingsGeneral.html), [SettingsRuntime.html](SettingsRuntime.html), [SettingsAbout.html](SettingsAbout.html) |

## Layout

- **The main window** follows Kyokai: a sidebar on the window's base (Mica) and, beside it, panes
  floating 8 points from the window's edges and from each other. The sidebar has **Add APK…** at
  the top, Library and Diagnostics, then **Open apps** — one row per app that has a pane or a
  window, saying where it is ("In the pane", "In its own window", "Couldn't start"), a click
  showing it — and at the bottom Settings and the runtime's state as a dot and a few words
  ("Ready to run apps", "Can't run apps"), which opens Diagnostics.
- **The Library pane** lists the apps: the APK's icon and label, one plain line on what it is
  ("Java only", "Java + native code · OpenGL ES") or why it can't run, a capability chip where
  something is limited, the state, and a **split button**: Open (Show while it runs) and ▾.
  An app that can't run here has Details instead of Open, never an Open that fails.
- **The phone pane** is a second pane to the right of the Library, 420 points wide, present while
  an app is shown in it. Its 44-point header (as Kyokai's pane header): the app's icon and name,
  its state ("Running · in the pane"), then Open in its own window, Stop and ⋯. Below it, on a
  soft background, a device-like frame holding the app's surface at the chosen screen size, and
  a small bar: Back (also Esc), Rotate, and the screen size. There is no fake Android status or
  navigation bar: nothing of an Android system is underneath. The pane shows one app; other apps
  opened into it keep running and are switched from Open apps.
- **Its own window** is Kyokai's "window of its own": one app, no sidebar, no frame or margin;
  the app's surface fills it. The header has the icon and name, Back, Rotate, **Move into the
  pane**, ⋯, then minimize, maximize and close. Closing the window stops that app (its tooltip
  says so).
- **Choosing how an app opens.** Each app has a default (Settings › General sets it for new apps;
  the app's details change it, and Add an app asks once). **Open** uses the default; **▾** offers
  Open in the pane (marked Default) and Open in its own window — once, without changing the
  default — and Change how it opens…. While it runs, the pane's Open in its own window and the
  window's Move into the pane move it between the two without restarting it, as do the
  right-click menu's items. One app is one runtime process, wherever it shows.
- **Stop and force stop.** Stop asks the app to close; Force stop (⋯ or right-click) ends its
  process at once. When Stop gets no answer within a few seconds, the pane or window says the app
  isn't responding and offers Force stop.
- **Settings** is a sheet over the whole window, as in Kyokai and Nextcloud MyGo: search, then
  Preferences › General; Running apps › Runtime and a link to Diagnostics; Windows-ARM64.apk ›
  About.
- **Capability honesty.** Wording says what works in plain terms ("Can run: Java only", "Native
  code support is experimental and off", "Vulkan for apps isn't available yet"), never error
  codes, guest offsets, hashes or internal paths. Only Diagnostics shows those, with `doctor`'s
  fields and the stable error categories of `PLAN.md` §14.2. A fixed graphics backend is never
  switched; the copy says so where it is chosen. Process isolation is described as crash
  handling, never as a sandbox. Add an app says that nothing in the file has run.
- **Not designed, on purpose:** a store, update prompts, and any file-association prompt.

## Tokens

The same as Nextcloud MyGo and Kyokai.

| Token | Light | Dark |
|---|---|---|
| Window base (Mica) | `#E9EEF3` | `#1A1C21` |
| Pane | `#FFFFFF` | `#23262C` |
| Border | `#DCE2E8` | `#33363E` |
| Text | `#1B1D21` | `#EDEFF2` |
| Secondary text | `#4A5260` | `#A9AFBA` |
| Accent (fills with white text) | `#0072B5` | `#4DAEE8` (with dark text) |
| Accent text and links | `#0069A6` | `#7CC6F2` |
| Success | `#0F7B3F` | `#4CC38A` |
| Needs attention, experimental | `#8A4B00` on `#FFF6E5` | `#F0B65A` on `#352812` |
| Danger, can't run | `#B42318` on `#FDEDEB` | `#F2877D` on `#3A1E1C` |

Type is Segoe UI Variable; sizes 12 / 13 / 14 body, 15–16 section titles, 20–24 page titles;
diagnostics values in Cascadia Mono. Radii: 6 controls, 8 panes, cards and menus, 10 dialogs and
the sheet; the phone frame 30 outside, 22 at the screen. The window keeps an 8-point margin
around its panes. The product icon drawn here — a window holding a phone, on the accent — is a
placeholder of Windows-ARM64.apk's own; it is never the Mac.apk logo or the Android robot. App
icons are stand-ins for the icons the APKs carry. Inside the frame and the app's window, the app's
own drawing is not bound by these tokens.
