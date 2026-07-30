<p align="center">
  <img src="assets/logo.png" alt="DualityRF logo" width="220">
</p>

# DualityRF

A local desktop SDR laboratory built with Qt 6, C++20, SoapySDR and FFTW.
Record, visualize, analyze and replay IQ sample streams from any
SoapySDR-compatible device (RTL-SDR, HackRF, ADALM-PLUTO, LimeSDR, BladeRF,
…). Fully offline: all data lives in local session directories.

## Features

- Runtime discovery of SoapySDR hardware, classified as receive-capable,
  transmit-capable or full duplex — no vendor-specific logic outside the
  adapter layer.
- Recording stages with configurable frequency, sample rate, bandwidth,
  gains and trigger mode; monitor mode for tuning before committing a
  recording, which then runs until stopped. Auto-capture carves each in-band
  transmission into its own trimmed file.
- Live FFT spectrum (zoom, pan, peak hold) and scrolling waterfall,
  processed off the UI thread.
- Optional concurrent transmission during capture: white/Gaussian noise,
  CW, offset sine or a user-supplied IQ waveform.
- Structured sessions (`Sessions/Session_YYYY-MM-DD_HH-MM-SS/`) with
  `recording_NNN.iq` (cs16), `metadata.json`, `session.json` and an
  `fft.cache` of average spectra.
- Playback of any recording through a selected transmitter with frequency,
  gain, rate, repeat and speed controls.
- Standalone Jam program: a dedicated transmitter (preset frequencies,
  waveform type, TX gain) with an always-on affected-bandwidth overlay and
  simultaneous FFT + waterfall.
- Standalone Sentry program: a reactive (triggered) jammer that watches the
  receiver for a transmission in a chosen window and, the instant the in-band
  peak crosses a threshold, fires a timed jamming burst on the transmitter,
  then holds off for a cooldown before re-arming — the classic RX → decision →
  TX loop, with detection paused during each burst so it never re-triggers on
  its own signal.
- Standalone Repeater program: an in-memory store-and-forward relay. It listens
  on a channel, buffers the raw IQ of each detected burst (power detector with
  hang time), band-limits it to a configurable capture range, then retransmits
  it once — optionally shifted by a carrier offset — so a weak signal is picked
  up and rebroadcast to reach further. Detected signals (within the capture
  range) are marked with a peak line and a toast. The receiver keeps running
  during the retransmit, so the FFT/waterfall show the replayed signal live —
  which needs a full-duplex device or a separate receiver and transmitter.
- Debug program: A/B comparison of recordings with overlaid FFTs, offline
  waterfall, side-by-side metadata, occupied bandwidth (99 %), mean/peak
  power and peak offset measurements.
- Monochrome, touch-friendly UI for a fixed 320×480 portrait display: a
  home-grid launcher of modular programs (FOB / JAM / DEBUG / ABOUT), each
  reachable via a slim back bar. Portable to ARM/Raspberry Pi.

## Getting started

### Prerequisites

You need a C++20 compiler (GCC 12+/Clang 15+), CMake ≥ 3.21, Ninja and the
following libraries:

| Dependency | Purpose |
|---|---|
| **Qt 6** (Widgets) | user interface |
| **SoapySDR** | vendor-neutral SDR hardware access |
| **FFTW3** (single precision) | FFT engine for spectrum/waterfall |
| **SoapyHackRF** | SoapySDR driver module for HackRF |
| **SoapyRTLSDR** | SoapySDR driver module for RTL-SDR |

SoapySDR alone only provides the API — you also need the driver module for
each device family you want to use. HackRF and RTL-SDR are the tested
targets; any other SoapySDR module (PlutoSDR, LimeSDR, BladeRF, …) should
work the same way.

#### Arch Linux

```sh
sudo pacman -S --needed base-devel cmake ninja qt6-base fftw soapysdr \
    soapyhackrf soapyrtlsdr
```

#### Debian / Ubuntu

```sh
sudo apt install build-essential cmake ninja-build qt6-base-dev \
    libfftw3-dev libsoapysdr-dev soapysdr-tools \
    soapysdr-module-hackrf soapysdr-module-rtlsdr
```

### Verify your hardware

Before building, check that SoapySDR sees your devices:

```sh
SoapySDRUtil --find
```

Each connected device should show up with its driver (`driver=hackrf`,
`driver=rtlsdr`, …). If a device is missing, its SoapySDR module is not
installed or udev denies access — on most distributions adding yourself to
the `plugdev`/`dialout` group or installing the `hackrf`/`rtl-sdr` udev
rules fixes the latter.

### Build

The project uses CMake presets (Ninja generator):

```sh
git clone https://github.com/Forsrobin/DualityRF.git
cd DualityRF
cmake --preset release
cmake --build --preset release
```

Use the `debug` preset for development builds (symbols, no optimization):

```sh
cmake --preset debug
cmake --build --preset debug
```

### Run

```sh
./build/release/src/duality_rf
```

Sessions and recordings are written to local `Sessions/` directories; no
network access is used.

## Contributing

Bug reports, hardware compatibility notes, features and documentation are
all welcome:

- Open bugs and feature requests on the
  [issue tracker](https://github.com/Forsrobin/DualityRF/issues).
- To contribute code, fork the repository, create a feature branch and open
  a pull request. Please keep changes buildable with the `release` preset.
- Commit messages must follow [Conventional Commits](docs/RELEASE.md#conventional-commits)
  (`feat` / `fix` / `chore`, with `!` for breaking changes). Run `npm install`
  once to enable the local commit-message hook.
- The design documents in [docs/](docs/) are the best starting point for
  understanding the codebase.

## Documentation

Design documents live in [docs/](docs/):
[architecture](docs/ARCHITECTURE.md) · [threading](docs/THREADING.md) ·
[storage format](docs/STORAGE.md) · [UI design](docs/UI.md) ·
[implementation plan](docs/IMPLEMENTATION_PLAN.md) ·
[release & versioning](docs/RELEASE.md).

## Layout

```
src/core      shared types, SPSC ring buffer (header-only)
src/sdr       ISDRDevice/IReceiver/ITransmitter, DeviceManager, SoapySDR adapter
src/dsp       FFT engine, windows, spectrum processing, waveform generators
src/storage   sessions, IQ file I/O (cs16/cf32 + .C16 import), fft cache
src/pipeline  capture / playback / concurrent-TX pipelines
src/ui        Qt Widgets front end
samples/      example capture (PortaPack .C16 + sidecar)
```
