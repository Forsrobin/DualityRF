# DualityRF — UI Design

## Target device & window

The UI is built for a **fixed 320 × 480 portrait touchscreen** (Raspberry Pi
class, Wayland/Hyprland). The main window is a fixed-size `QMainWindow`
(`kWindowWidth = 320`, `kWindowHeight = 480` in `MainWindow`); there is no
resizing or dock rearranging. Everything is designed for finger taps, not a
mouse.

## Style

Strict monochrome: black background (`#000000`), white foreground
(`#FFFFFF`), square corners, 1 px white borders, no gradients, generous hit
targets for touch. The only color in the application is inside the
spectrum/waterfall plot areas (intensity colormap) and the hazard-striped
START buttons. Implemented as one application-wide QSS in `ui/Theme.cpp`
(`Theme::apply`).

Two intentional exceptions to the pure-monochrome rule:

- **Home tiles** are white background / `#808080` gray border / black text
  (so the launcher reads as physical buttons).
- **Home footer labels** (version + clock) are fully transparent (no border,
  no background) so they sit cleanly over the footer's top border.

## Navigation model

The app is a single `QStackedWidget` (`m_stack` in `MainWindow`). There is
**no menu bar and no dock system** — navigation is splash → home → program →
back to home.

```
        ┌─────────────┐   1500 ms    ┌─────────────┐
launch ▶│ SplashPage  │ ───────────▶ │  HomePage   │
        │  (index 0)  │  singleShot  │  launcher   │
        └─────────────┘              └──────┬──────┘
                                      tap tile │  ▲ back arrow
                                               ▼  │
                                     ┌──────────────────────┐
                                     │    ProgramScreen     │
                                     │  ┌────────────────┐  │
                                     │  │ ‹  TITLE  (bar)│  │ 30 px back-bar
                                     │  ├────────────────┤  │
                                     │  │  program body  │  │
                                     │  └────────────────┘  │
                                     └──────────────────────┘
```

### `SplashPage` (`ui/SplashPage.*`)

The boot splash is the **first page of the window itself** (not a separate
`QSplashScreen`): logo (200 px), tagline `SDR CAPTURE · ANALYSIS · REPLAY`,
the app version, and "Starting up…", centered on black. `MainWindow` shows it
first, then `QTimer::singleShot(kSplashMinimumMs = 1500)` switches to the home
page.

### `HomePage` (`ui/HomePage.*`) — the launcher

```
┌──────────────────────────────┐
│ ┌────────┐┌────────┐┌────────┐│  3-column QGridLayout of tiles.
│ │  [icon]││  [icon]││  [icon]││  Each tile = QToolButton, 64 px tall,
│ │  FOB   ││  JAM   ││ DEBUG  ││  ToolButtonTextUnderIcon (icon over label).
│ └────────┘└────────┘└────────┘│
│ ┌────────┐                    │
│ │  [icon]│                    │
│ │ ABOUT  │                    │
│ └────────┘                    │
│              ...              │
├──────────────────────────────┤  32 px footer, top border only.
│ v2.0.2              14:30:05  │  version (left) · live HH:MM:SS clock (right)
└──────────────────────────────┘
```

- `addProgram(name, iconPath)` appends one tile; the index passed to the
  `programActivated(int)` signal matches the registration order (0, 1, 2…).
- The clock label ticks every 1000 ms via a `QTimer`
  (`QTime::currentTime().toString("HH:mm:ss")`).
- Adding a new program to the launcher is a **one-line** call in
  `MainWindow` — see "Adding a program" below.

### `ProgramScreen` (`ui/ProgramScreen.*`) — the back-bar wrapper

Every program's content is wrapped in a `ProgramScreen`, which prepends a
slim **30 px top bar**:

```
┌──────────────────────────────┐
│ ‹  FOB        [TX HackRF][RX —]│  back arrow + title (left), device badges (right)
├──────────────────────────────┤
```

- Left: a `‹` back arrow (fixed 38 × 24, 13 px font) and the program title.
  Tapping the arrow emits `backRequested()`, which `MainWindow` routes back to
  the home page. One wrapper guarantees the bar is identical across programs.
- Right: two **`DeviceBadge`** chips — red **TX** and blue **RX**
  (`ui/components/DeviceBadge.*`) — each showing the trimmed name of the
  selected device (or `—`). Tapping a badge emits `txClicked()` / `rxClicked()`,
  which opens the **Devicees** selector focused on that section. The badges
  appear on every program so the current devices are always visible;
  `MainWindow::refreshDeviceBadges()` mirrors the selection onto all of them.

The status bar is only shown while a `ProgramScreen` is current (hidden on
splash/home), handled in `MainWindow`'s `currentChanged` handler.

## Programs

Programs live in `src/ui/programs/` as `{Name}Program.{h,cpp}` and are
registered in `MainWindow` via `addProgram(name, iconPath, content)`, which
wraps the content in a `ProgramScreen`, pushes it on the stack, and adds the
home tile. Current programs:

| Tile     | Class            | Icon                    | Purpose |
|----------|------------------|-------------------------|---------|
| FOB      | `FobProgram`     | `:/assets/fob.png`      | Capture / monitor / concurrent-TX / sessions / playback — the main workflow |
| DEVICEES | `DevicesProgram` | `:/assets/devicees.png` | App-wide RX/TX device selector (see below) |
| JAM      | `JamProgram`     | `:/assets/jam.png`      | Standalone transmitter for testing / jamming |
| DEBUG    | `DebugProgram`   | `:/assets/bug.png`      | Offline A/B analysis of recordings |
| ABOUT    | `InfoProgram`    | `:/assets/info.png`     | App info / version |

### FOB program (`programs/FobProgram.*`)

The main workflow. A wrapping **`FlowLayout` tab row** selects one control
panel at a time from a `QStackedWidget`, shown above a switchable
waterfall/FFT visualization (`VizPanel`).

```
┌──────────────────────────────┐
│ ‹  FOB                       │  back-bar
├──────────────────────────────┤
│ [CAPTURE][CONCURRENT TX]     │  FlowLayout tab row (wraps as needed)
│ [SESSIONS][PLAYBACK]         │
├──────────────────────────────┤
│  (selected panel)            │  QStackedWidget of the 4 panels
│  Freq / Rate / BW / Gain …   │
├──────────────────────────────┤
│  [ WATERFALL | FFT ] toggle  │  VizPanel: one view at a time
│  ▒▒▓▓██▓▓▒▒ scrolling        │
└──────────────────────────────┘
```

`FobProgram` **owns** the panels and views; `MainWindow` reaches them through
accessors (`capturePanel()`, `transmitPanel()`, `playbackPanel()`,
`sessionBrowser()`, `spectrumView()`, `waterfallView()`) to wire them to the
pipelines. This alias-pointer pattern keeps all the capture/playback signal
wiring in `MainWindow` unchanged while the widgets themselves live in the
program. Device selection is **not** here — it moved to the Devicees program.

The FOB panels:

- **Capture** (`CapturePanel`) — frequency, sample rate, bandwidth, RX gain,
  trigger mode. `MONITOR` streams without writing; `RECORD` writes
  `recording_NNN.iq` into the current session. **Recording runs until you tap
  STOP — there is no duration field.** Also arms auto-capture / replay mode.
- **Concurrent TX** (`TransmitPanel`) — enable a generated waveform (white /
  Gaussian noise, CW, offset sine, IQ file) alongside a running capture:
  amplitude, offset from carrier, noise bandwidth, TX gain.
- **Sessions** (`SessionBrowser`) — browse sessions and their recordings.
- **Playback** (`PlaybackPanel`) — replay a recording through a TX device:
  frequency, gain, sample rate, repeat, speed (0.25×–4×).

### Devicees program (`programs/DevicesProgram.*`)

The **app-wide device selector** and the single source of truth for the
selected RX/TX devices — the pipelines read `selectedRx()` / `selectedTx()`
from it, and every program's top-bar badges mirror its `selectionChanged()`.

```
┌──────────────────────────────┐
│ ‹  DEVICEES        [TX][RX]   │
├──────────────────────────────┤
│ ▓█▓▒░   ░▒▓█▓  (Cylon sweep)  │  ScanIndicator — animated while scanning
│ [   SCAN FOR DEVICES   ]      │
├──────────────────────────────┤
│ RECEIVER            (blue)    │
│  ( ) (none)                   │  selectable rows, exclusive; selected row
│  (•) HackRF One [duplex]      │  is inverted (white) via the theme
│ TRANSMITTER         (red)     │
│  (•) HackRF One [duplex]      │
└──────────────────────────────┘
```

- Devices are shown as two **touch-friendly selectable lists** (RX and TX),
  not dropdowns; a full-duplex device appears in both. Selection persists via
  `QSettings` (`devices/rx`, `devices/tx`) and is restored after each scan.
- **`ScanIndicator`** (`ui/components/ScanIndicator.*`) is a monochrome
  "Cylon" sweep that animates during a scan for a bit of cool factor, and
  settles to a dim static row when idle.
- Opened either from its home tile or by tapping a top-bar badge (which scrolls
  the matching section into view via `focusReceiver()` / `focusTransmitter()`).

**History-aware back navigation:** unlike other programs (whose back arrow
always returns home), Devicees returns to *whatever screen opened it*.
`MainWindow::openDevices()` records the current stack index in
`m_devicesReturnIndex`, and the Devicees back arrow returns there. So
`home → FOB → (tap RX badge) → Devicees → back → FOB`, but
`home → Devicees → back → home`.

### JAM program (`programs/JamProgram.*`)

A **standalone transmitter**, built to exercise the program modularity — it
owns its controls and its own visualization and is not wired into the FOB
capture flow.

```
┌──────────────────────────────┐
│ ‹  JAM                       │
├──────────────────────────────┤
│ Preset ▾   Frequency [____]  │  preset dropdown fills frequency
│ Sample rate [____]           │
│ TX gain  [======]            │  gain only — no power-% control
│ Type ▾  (WhiteNoise/Gauss/   │  matches FOB concurrent-TX types
│          CW/Sine/IqFile)     │
│ Offset [±kHz]  BW [kHz]      │
│ File […] (IqFile only)       │
├──────────────────────────────┤
│ ▓▓▓ START (always hazard) ▓▓ │  pinned above the viz, always striped
├──────────────────────────────┤
│  FFT  + affected-band overlay│  BOTH shown at once (unlike FOB)
│  ▒▒▓▓██▓▓▒▒ WATERFALL        │
└──────────────────────────────┘
```

Distinct JAM behaviors:

- The **affected bandwidth is always drawn** as an overlay band on the FFT
  (`SpectrumWidget::setCaptureRange`), centered at `frequency + offset` with
  width = configured bandwidth — even before transmitting — so the operator
  always sees what will be jammed.
- **Waterfall and FFT are shown simultaneously** at the bottom (the FOB
  toggles between them; JAM stacks both).
- The **START button is permanently hazard-striped**
  (`HazardButton::setAlwaysHazard(true)`), pinned directly above the viz.
- When a receiver is selected, `MainWindow::startJam()` opens the TX device
  and an optional RX monitor (`m_jamMonitor` → `m_jamProcessor` →
  `JamProgram::pushSpectrum`) so the transmission is visualized live. Leaving
  the JAM screen stops it (`currentChanged` handler).

### DEBUG program (`programs/DebugProgram.*`)

Offline A/B analysis of recordings: open any session, pick recordings A/B,
overlaid average FFTs (A white, B gray) with zoom, per-recording waterfall
rendered from file, side-by-side metadata, and measured values (occupied
bandwidth 99 %, mean/peak power, peak frequency offset). Refreshed when its
screen becomes current.

## Shared UI building blocks

Reusable pieces live under `src/ui/`:

| Location       | Piece            | Role |
|----------------|------------------|------|
| `components/`  | `FlowLayout`     | Wrapping horizontal layout (the FOB tab row) |
| `components/`  | `HazardButton`   | Yellow/black hazard-striped button; striped when checked, or always via `setAlwaysHazard(true)` |
| `components/`  | `ToastManager`   | Transient status/error toasts |
| `components/`  | `DeviceBadge`    | Clickable red-TX / blue-RX top-bar chip showing the trimmed device name |
| `components/`  | `ScanIndicator`  | Animated "Cylon" sweep shown while scanning for devices |
| `widgets/`     | `SpectrumWidget` | Live FFT plot: dB/freq axes, peak hold, zoom/pan, capture-range overlay band |
| `widgets/`     | `WaterfallWidget`| Scrolling waterfall (circular `QImage`, newest row on top) |
| `panels/`      | `VizPanel`       | Container that toggles between the spectrum and waterfall |
| ui root       | `Theme`          | The single application-wide QSS + palette |

> **Portability note:** `WaterfallWidget` uses a `QT_VERSION`-guarded
> `flippedVertical()` helper — `QImage::flipped()` is Qt 6.9+, so it falls
> back to `mirrored(false, true)` on the Qt 6.4 shipped by the CI runners
> (Ubuntu 24.04). Keep new UI code within the Qt 6.4 API or guard it likewise.

## Adding a program

The launcher is modular by design. To add a program:

1. Create `src/ui/programs/MyProgram.{h,cpp}` (a `QWidget`), and add the two
   files to `src/ui/CMakeLists.txt`.
2. Add its icon to `assets/` and register it in `src/resources.qrc`.
3. In `MainWindow`, one call: `addProgram(tr("MYPROG"), ":/assets/myprog.png",
   new MyProgram(this));` — this adds the home tile **and** the back-barred
   stack page. If the program needs start/stop-on-navigation behavior, hook it
   in the `currentChanged` handler like JAM does.
