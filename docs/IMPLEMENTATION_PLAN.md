# DualityRF — Incremental Implementation Plan

Each phase must configure and compile (`cmake --preset debug && cmake --build
--preset debug`) before the next begins.

| Phase | Deliverable | Contents |
|-------|-------------|----------|
| 1 | Build skeleton | Root + per-module CMake, C++20, `duality_core`: `Types.h` (StreamParams, Range, SampleFormat), `RingBuffer.h` (SPSC), `Version.h`; app stub links Qt |
| 2 | SDR layer | `ISDRDevice`, `IReceiver`, `ITransmitter`, `DeviceInfo`, `DeviceManager` (SoapySDR enumerate + classify), `SoapyDevice` adapter |
| 3 | DSP | `Window`, `FftEngine` (FFTW3f), `SpectrumProcessor` (worker QObject), `WaveformGenerator` (white/gaussian/CW/sine/file), `Measurements` (OBW, power) |
| 4 | Storage | `RecordingMetadata` (JSON), `Session`, `SessionStore`, `IqFileWriter`, `IqFileReader` (cs16/cf32, `.C16`+`.TXT` import), `FftCache` |
| 5 | Pipelines | `CapturePipeline` (monitor/record, duration, triggers), `PlaybackPipeline` (repeat, speed), `TransmitPipeline` (concurrent waveform TX) |
| 6 | UI | `Theme`, `SpectrumWidget`, `WaterfallWidget`, `DevicePanel`, `CapturePanel`, `TransmitPanel`, `PlaybackPanel`, `SessionBrowser`, debug workspace, `MainWindow` |
| 7 | Integration | `main.cpp`, release build, README |

Out of scope for the first iteration (kept possible by the architecture):
plugin loading from shared objects (adapters are compiled in; the interface
boundary is already plugin-shaped), GPU waterfall.

## Evolution since the initial plan

The layers above are intact, but the app has moved on from the phase-6/7
sketch. The current state (see [UI.md](UI.md), [ARCHITECTURE.md](ARCHITECTURE.md),
[RELEASE.md](RELEASE.md)):

- **Home-launcher UI, not docks.** The dock/menu layout was replaced by a
  fixed 320×480 `QStackedWidget`: boot `SplashPage` → `HomePage` grid launcher
  → `ProgramScreen`-wrapped programs. `src/ui/` is split into
  `components/ · widgets/ · panels/ · programs/`.
- **Programs are modular.** Feature screens live in `src/ui/programs/` as
  `{Name}Program`: `FobProgram` (the phase-6 workflow), `DebugProgram`
  (renamed from the debug workspace), `InfoProgram` (About), and `JamProgram`
  (a new standalone transmitter added to prove the program model).
- **Triggered capture shipped.** The "triggered capture on signal threshold"
  item is implemented as **auto-capture + replay mode** (`PeakDetector`
  presence test, `CapturePipeline` segments, `BandTrimmer` trimming).
- **Capture duration removed** from the UI — recordings run until STOP.
- **HackRF gain fix** — `SoapyDevice::applyGain()` puts the requested TX gain
  on the widest-range element so the reported gain matches the set value.
- **Release automation added** — semantic-versioning CI/CD pipeline; see
  [RELEASE.md](RELEASE.md).
