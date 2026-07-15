# DualityRF — UI Design

## Style

Strict monochrome: black background (`#000000`), white foreground
(`#FFFFFF`), square corners, 1 px white borders, no gradients, no icons in
controls, generous hit targets (min 40 px) for touch. The only color in the
application is inside the spectrum/waterfall plot areas (intensity
colormap). Implemented as one application-wide QSS in `ui/Theme.cpp`.

## Main window (dockable layout)

```
┌──────────────────────────────────────────────────────────────────────┐
│ DUALITY RF          [ CAPTURE ] [ PLAYBACK ] [ DEBUG ]               │
├──────────────┬───────────────────────────────────────────────────────┤
│ DEVICES      │  SPECTRUM (FFT)                                       │
│ ┌──────────┐ │  ┌─────────────────────────────────────────────────┐  │
│ │RX ▾      │ │  │        ╱╲      dB axis, peak hold,              │  │
│ │HackRF One│ │  │  ──╱╲─╱  ╲──╱╲─── freq axis, zoom (wheel/drag)  │  │
│ │TX ▾      │ │  └─────────────────────────────────────────────────┘  │
│ │(none)    │ │  WATERFALL                                            │
│ │[RESCAN]  │ │  ┌─────────────────────────────────────────────────┐  │
│ └──────────┘ │  │ ▒▒▓▓██▓▓▒▒  scrolling history, shared freq axis │  │
│ CAPTURE      │  │ ▒▒▒▓██▓▒▒▒                                      │  │
│ ┌──────────┐ │  └─────────────────────────────────────────────────┘  │
│ │Freq [__] │ ├───────────────────────────────────────────────────────┤
│ │Rate [__] │ │ SESSIONS                                              │
│ │BW   [__] │ │ Session_2026-07-15_14-30-05                           │
│ │Gain [==] │ │   ├ recording_001.iq   433.8 MHz  1 MS/s  10 s        │
│ │Dur  [__] │ │   └ recording_002.iq   ...                            │
│ │[MONITOR] │ │ [OPEN] [PLAY] [COMPARE]                               │
│ │[RECORD ] │ │                                                       │
│ └──────────┘ │                                                       │
└──────────────┴───────────────────────────────────────────────────────┘
```

Panels are `QDockWidget`s: Devices, Capture, Concurrent TX, Playback,
Sessions — all dockable/floatable/closable; Spectrum + Waterfall form the
central widget. Layout persists via `QMainWindow::saveState`.

## Capture panel (recording stage)

Receiver (from Devices panel selection), optional transmitter, frequency,
sample rate, bandwidth, RX gain, TX gain (when concurrent TX enabled),
duration (0 = until stopped), trigger mode (Manual / Auto on start).
`MONITOR` streams without writing; `RECORD` arms the stage and writes
`recording_NNN.iq` into the current session.

## Concurrent TX panel

Enable checkbox; generator: White noise / Gaussian noise / CW / Sine /
IQ file; amplitude; frequency offset from the carrier (applies to every
generator, e.g. 433.8 MHz + 100 kHz emits around 433.9 MHz); range
(bandwidth) for the noise generators (0 = full band); TX gain. Runs while a
capture stage — monitor or record — is active.

## Playback panel

Recording (from Sessions selection), output SDR, frequency (defaults to the
recording's), gain, sample rate (defaults to recording's), repeat, speed
(0.25×–4× where hardware allows). `TRANSMIT` / `STOP`.

## Debug workspace

Separate top-level view (toolbar toggle) for offline analysis:

```
┌──────────────────────────────────────────────────────────────────────┐
│ DEBUG   [OPEN SESSION]  A: recording_001  B: recording_002 [SWAP]    │
├───────────────────────────────┬──────────────────────────────────────┤
│ FFT OVERLAY  (A white, B gray)│ METADATA            A        B       │
│  ┌─────────────────────────┐  │ frequency       433.8 MHz  433.8 MHz │
│  │   ╱╲    ╱╲   zoom+pan   │  │ sample rate     1 MS/s     1 MS/s    │
│  └─────────────────────────┘  │ duration        10.0 s     8.2 s     │
│ WATERFALL (selected rec)      │ occupied BW(99%) 18.4 kHz  17.9 kHz  │
│  ┌─────────────────────────┐  │ mean power     -48.2 dBFS -47.1 dBFS │
│  │                         │  │ peak power     -21.0 dBFS -20.4 dBFS │
│  └─────────────────────────┘  │ started         12:30:05   12:31:12  │
│ [REPLAY A] [REPLAY B]         │ Δf at peak      +1.2 kHz             │
└───────────────────────────────┴──────────────────────────────────────┘
```

Capabilities: open any session; select recordings A/B; overlaid average
FFTs with zoom; per-recording waterfall rendered from file; metadata
side-by-side; measured values (occupied bandwidth 99 %, mean/peak power,
peak frequency offset); replay either recording through the playback
pipeline; timing info from metadata.
