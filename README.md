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
  gains, duration and trigger mode; monitor mode for tuning before
  committing a recording.
- Live FFT spectrum (zoom, pan, peak hold) and scrolling waterfall,
  processed off the UI thread.
- Optional concurrent transmission during capture: white/Gaussian noise,
  CW, offset sine or a user-supplied IQ waveform.
- Structured sessions (`Sessions/Session_YYYY-MM-DD_HH-MM-SS/`) with
  `recording_NNN.iq` (cs16), `metadata.json`, `session.json` and an
  `fft.cache` of average spectra.
- Playback of any recording through a selected transmitter with frequency,
  gain, rate, repeat and speed controls.
- Debug workspace: A/B comparison of recordings with overlaid FFTs, offline
  waterfall, side-by-side metadata, occupied bandwidth (99 %), mean/peak
  power and peak offset measurements.
- Monochrome, touch-friendly UI with dockable panels; color appears only in
  spectrum visualizations. Portable to ARM/Raspberry Pi.

## Building

Dependencies: CMake ≥ 3.21, Ninja, Qt 6 (Widgets), SoapySDR, FFTW3 (single
precision), plus SoapySDR driver modules for your hardware.

```sh
cmake --preset release
cmake --build --preset release
./build/release/src/duality_rf
```

Use the `debug` preset for development builds.

## Documentation

Design documents live in [docs/](docs/):
[architecture](docs/ARCHITECTURE.md) · [threading](docs/THREADING.md) ·
[storage format](docs/STORAGE.md) · [UI design](docs/UI.md) ·
[implementation plan](docs/IMPLEMENTATION_PLAN.md).

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
