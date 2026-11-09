#!/usr/bin/env python3
"""
ETCH -- A Generative Acid Machine -- by Synth Centric
Wavetable + transfer-LUT generator.

The Game Boy wave channel (CH3) plays a 32-sample, 4-bit waveform.  It has no
filter.  ETCH approximates a resonant low-pass filter by pre-rendering, for
each oscillator personality, a family of single-cycle waveforms as they would
look after passing through a resonant 4-pole ladder low-pass filter at 32
cutoff positions and 8 resonance amounts.

Method (all original, computed offline, fully reproducible):

  1. Build the complex harmonic spectrum (harmonics 1..15; harmonic 16 is the
     Nyquist bin of a 32-sample cycle and is left empty) of each oscillator.
  2. Multiply every harmonic n by the frequency response of a 4-pole ladder:
         G(n)  = 1 / (1 + j * n / kc)            (one pole, kc = cutoff in
                                                  harmonic units)
         H(n)  = G^4 / (1 + k * G^4)             (k = resonance feedback)
     The cutoff is expressed relative to the fundamental, i.e. the filter is
     key-tracked: a table index produces the same timbre at every pitch.
  3. Inverse FFT to 32 samples, normalise (with a deliberate loss of level as
     the cutoff closes below the fundamental, as a real filter would) and
     quantise to 4 bits.

A second table set ("transfer LUTs") maps every possible wave-RAM byte (two
4-bit samples) through a drive curve (OFF / SOFT / HARD) and one of 8
amplitude levels.  The audio engine runs the 16 wavetable bytes through one
of these 256-byte tables when it loads wave RAM, which gives a VCA envelope,
accent loudness and distortion without any real-time arithmetic.

Outputs (C sources for GBDK-2020, autobanked):
   assets/generated_waves/wavetables.c   16384 bytes  [osc][res][cut][16]
   assets/generated_waves/wavelut.c       6144 bytes  [drive][amp][256]
   assets/generated_waves/wavetables.bin / wavelut.bin  raw copies
"""
import math
import os
import sys

import numpy as np

N_SAMPLES = 32
N_HARM = 15            # highest harmonic representable below Nyquist
N_OSC = 4              # SAW, SQUARE, HOLLOW, BITE
N_RES = 8
N_CUT = 32
N_DRIVE = 3            # OFF, SOFT, HARD
N_AMP = 8              # 0 = loudest

# cutoff index -> cutoff frequency in harmonics of the played note
KC_MIN = 0.55
KC_MAX = 18.0
# resonance index -> ladder feedback (4.0 would self-oscillate)
RES_K = [0.0, 0.9, 1.6, 2.2, 2.7, 3.1, 3.4, 3.65]
# amplitude levels (linear), index 0 = full
AMP_LEVELS = [1.00, 0.84, 0.70, 0.58, 0.47, 0.37, 0.27, 0.17]


def osc_spectrum(osc):
    """Complex spectrum X[0..16] (rfft layout, 32-point) of one oscillator."""
    t = np.arange(4096) / 4096.0
    if osc == 0:      # SAW: classic falling ramp, all harmonics 1/n
        x = 1.0 - 2.0 * t
    elif osc == 1:    # SQUARE: slightly asymmetric pulse with a sloped top,
        # which keeps a little even-harmonic content (nasal, not a pure square)
        x = np.where(t < 0.47, 1.0, -1.0) * 0.82 + (1.0 - 2.0 * t) * 0.18
    elif osc == 2:    # HOLLOW: rounded odd-harmonic tone (soft triangle/square)
        sq = np.where(t < 0.5, 1.0, -1.0)
        tri = 1.0 - 4.0 * np.abs(t - 0.5)
        x = 0.45 * sq + 0.55 * tri
    else:             # BITE: hard-synced ramp (slave ratio 2.6) + ramp
        slave = 1.0 - 2.0 * ((t * 2.6) % 1.0)
        x = 0.6 * slave + 0.4 * (1.0 - 2.0 * t)
    x = x - x.mean()
    full = np.fft.rfft(x) / len(x)
    spec = np.zeros(N_SAMPLES // 2 + 1, dtype=complex)
    spec[1:N_HARM + 1] = full[1:N_HARM + 1]
    if osc == 2:
        # HOLLOW: extra harmonic roll-off keeps it restrained
        for n in range(1, N_HARM + 1):
            spec[n] *= 1.0 / (n ** 0.35)
    return spec


def ladder_response(n, kc, k):
    g = 1.0 / (1.0 + 1j * n / kc)
    g4 = g ** 4
    return g4 / (1.0 + k * g4)


def cutoff_kc(c):
    return KC_MIN * (KC_MAX / KC_MIN) ** (c / (N_CUT - 1))


def level_target(c):
    """Output level as the cutoff closes (keeps loudness musically stable
    while the filter is open, falls away once it closes on the fundamental)."""
    kc = cutoff_kc(c)
    if kc >= 1.6:
        return 1.0
    return max(0.42, 0.42 + 0.58 * (kc - KC_MIN) / (1.6 - KC_MIN))


def render(osc, res, cut):
    spec = osc_spectrum(osc)
    kc = cutoff_kc(cut)
    k = RES_K[res]
    out = np.zeros_like(spec)
    for n in range(1, N_HARM + 1):
        # passband-loss compensation of the ladder ((1+k) DC gain loss)
        out[n] = spec[n] * ladder_response(n, kc, k) * (1.0 + 0.55 * k)
    x = np.fft.irfft(out, N_SAMPLES) * N_SAMPLES
    peak = np.max(np.abs(x))
    if peak < 1e-9:
        return [8] * N_SAMPLES
    x = x / peak * level_target(cut)
    q = np.clip(np.round(7.5 + 7.5 * x), 0, 15).astype(int)
    return [int(v) for v in q]


def pack(samples):
    return bytes(((samples[2 * i] & 15) << 4) | (samples[2 * i + 1] & 15)
                 for i in range(N_SAMPLES // 2))


def drive_curve(d, x):
    if d == 0:
        return x
    if d == 1:   # SOFT: tanh saturation, renormalised
        return math.tanh(2.3 * x) / math.tanh(2.3)
    # HARD: strong gain into a hard clip, with a touch of asymmetry (digital)
    y = 3.2 * x + 0.12
    return max(-1.0, min(1.0, y))


def nib_map(d, a, v):
    x = (v - 7.5) / 7.5
    y = drive_curve(d, x) * AMP_LEVELS[a]
    return int(max(0, min(15, round(7.5 + 7.5 * y))))


def c_array(name, data, bank_comment):
    lines = []
    lines.append("/* AUTOGENERATED by tools/generate_waves.py -- do not edit. */")
    lines.append("/* ETCH -- A Generative Acid Machine -- by Synth Centric   */")
    lines.append("#pragma bank 255")
    lines.append("#include <gbdk/platform.h>")
    lines.append("#include <stdint.h>")
    lines.append("BANKREF(%s)" % name)
    lines.append("/* %s */" % bank_comment)
    lines.append("const uint8_t %s[%d] = {" % (name, len(data)))
    for i in range(0, len(data), 16):
        lines.append("  " + ",".join("0x%02X" % b for b in data[i:i + 16]) + ",")
    lines.append("};")
    return "\n".join(lines) + "\n"


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else "assets/generated_waves"
    os.makedirs(outdir, exist_ok=True)
    waves = bytearray()
    for osc in range(N_OSC):
        for res in range(N_RES):
            for cut in range(N_CUT):
                waves += pack(render(osc, res, cut))
    assert len(waves) == 16384
    lut = bytearray()
    for d in range(N_DRIVE):
        for a in range(N_AMP):
            for b in range(256):
                hi = nib_map(d, a, b >> 4)
                lo = nib_map(d, a, b & 15)
                lut.append((hi << 4) | lo)
    assert len(lut) == 6144
    with open(os.path.join(outdir, "wavetables.c"), "w") as f:
        f.write(c_array("etch_wavetables", waves,
                        "layout: [osc 4][res 8][cut 32][16 bytes]"))
    with open(os.path.join(outdir, "wavelut.c"), "w") as f:
        f.write(c_array("etch_wavelut", lut,
                        "layout: [drive 3][amp 8][256]  byte -> byte"))
    with open(os.path.join(outdir, "wavetables.bin"), "wb") as f:
        f.write(waves)
    with open(os.path.join(outdir, "wavelut.bin"), "wb") as f:
        f.write(lut)
    print("wavetables: %d bytes, luts: %d bytes" % (len(waves), len(lut)))


if __name__ == "__main__":
    main()
