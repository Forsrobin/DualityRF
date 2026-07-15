# DualityRF — Storage Format

## Directory layout

```
<data root>/Sessions/
    Session_2026-07-15_14-30-05/
        recording_001.iq
        recording_002.iq
        metadata.json          # array of per-recording metadata
        session.json           # session-level settings
        fft.cache              # cached average spectra (binary)
```

The data root defaults to `~/DualityRF` and is configurable in the UI.

## IQ files (`recording_NNN.iq`)

Raw interleaved samples, no header (all description lives in
`metadata.json`). Native recording format is **cs16** — interleaved signed
16-bit little-endian I,Q pairs, full-scale ±32767. Rationale: half the disk
and I/O bandwidth of float (Raspberry Pi friendly) and identical to the
PortaPack/HackRF `.C16` convention, so existing captures (e.g.
`BBD_0001.C16` + its `.TXT` sidecar) import directly.

`IqFileReader` also reads **cf32** (interleaved float32) so foreign
recordings can be analyzed; the `format` field in metadata selects the
decoder.

## `metadata.json`

JSON array; one object per recording:

```json
[
  {
    "file": "recording_001.iq",
    "format": "cs16",
    "device": { "driver": "hackrf", "label": "HackRF One #0",
                "serial": "0000...c63c" },
    "frequencyHz": 433800000.0,
    "sampleRateHz": 1000000.0,
    "bandwidthHz": 1750000.0,
    "rxGainDb": 32.0,
    "txGainDb": null,
    "startedUtc": "2026-07-15T12:30:05Z",
    "durationSec": 10.0,
    "samples": 10000000,
    "trigger": "manual",
    "concurrentTx": null,
    "softwareVersion": "2.0.0"
  }
]
```

`concurrentTx`, when a waveform was transmitted during capture, records the
generator type and its parameters.

## `session.json`

```json
{
  "created": "2026-07-15T12:30:05Z",
  "name": "Session_2026-07-15_14-30-05",
  "notes": "",
  "softwareVersion": "2.0.0"
}
```

## `fft.cache`

Binary cache of one average power spectrum per recording so the session
browser and debug workspace can show previews without re-reading gigabytes
of IQ. Format (little-endian):

```
magic   u32  'DFFT' (0x54464644)
version u32  1
entries:
    nameLen u32, name  utf8[nameLen]     # e.g. "recording_001.iq"
    bins    u32
    data    f32[bins]                    # dBFS, fftshifted
```

Entries are appended when a recording finalizes; a missing/stale cache is
simply regenerated from the IQ file on demand.
