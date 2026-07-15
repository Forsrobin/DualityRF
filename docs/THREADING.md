# DualityRF — Threading Model

## Threads

| Thread              | Owner                | Work                                            | Lifetime |
|---------------------|----------------------|-------------------------------------------------|----------|
| Qt main (UI)        | QApplication         | widgets, painting, user input                   | app      |
| Discovery worker    | DeviceManager        | SoapySDR enumerate + open (can take seconds)    | per scan |
| RX worker           | CapturePipeline      | `IReceiver::read` loop, file writing            | per capture |
| Spectrum worker     | SpectrumProcessor    | window + FFT + averaging, emits dB rows         | per capture/playback view |
| TX worker (playback)| PlaybackPipeline     | file read → `ITransmitter::write` loop          | per playback |
| TX worker (waveform)| TransmitPipeline     | generator → `ITransmitter::write` loop          | per concurrent-TX stage |

Workers are `std::jthread`s with `std::stop_token` cooperative cancellation.
Pipelines are `QObject`s living on the main thread; workers communicate
results exclusively by emitting signals (cross-thread emission ⇒ Qt queues
them onto the main thread automatically) — no widget is ever touched off the
main thread.

## Data paths and synchronization

1. **RX → display**: lock-free SPSC ring buffer (`core/RingBuffer.h`).
   Producer = RX worker, consumer = spectrum worker. On overflow the oldest
   display samples are dropped; recording is unaffected.
2. **RX → disk**: written inline on the RX worker through a buffered
   `IqFileWriter` (64 KiB chunks). Disk latency spikes are absorbed by the
   device's own stream buffering; a slow disk surfaces as reported overflows,
   never as a corrupted file.
3. **Workers → UI**: value-type payloads only (`QVector<float>`, metadata
   structs) via queued signals. No shared mutable state with the UI.
4. **Stop**: `request_stop()` → worker drains, finalizes the file, emits a
   completion signal, joins in the pipeline's `stop()`/destructor. UI buttons
   stay responsive because joining happens after the read loop notices the
   stop token (bounded by one stream-read timeout, ≤ 100 ms).

## Rules

- SoapySDR device handles are used by one worker at a time per direction
  (RX stream on the RX worker, TX stream on a TX worker). Tuning while a
  stream runs goes through the adapter, which serializes with a mutex.
- FFTW plan creation is not thread-safe → guarded by a global mutex in
  `FftEngine`; `fftwf_execute` on distinct plans is safe concurrently.
- Everything the UI reads is either a copy delivered by a signal or an
  immutable file on disk.
