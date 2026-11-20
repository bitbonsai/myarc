"""Generate the myArc welcome-intro soundscape from scratch.

Deterministic (fixed seed). Writes 48 kHz 16-bit stereo WAV.
Run: uv run --with numpy --with scipy python make_welcome_sound.py [out.wav]

Timeline (t=0 is playback start):
  0.0-0.4  silence into a soft fade-in
  0.4-1.6  airy build: low-passed pad + breathy noise swell rising with the blob
  1.6      reveal 1: low thump, filter opens, warm bell bloom (stronger accent)
  1.7-4.0  sustain under the logo animation
  4.3      reveal 2: lighter thump and bell, chord voices glide to the next chord
  4.5-10   gentle bed under the welcome buttons
  10-13.5  fade-out, reverb tail gone by ~13.6 s
"""
import sys
import numpy as np
from scipy import signal

SR = 48000
DUR = 14.0
N = int(SR * DUR)
T = np.arange(N) / SR
SEED = 20261120
rng = np.random.default_rng(SEED)

T_REV1 = 1.6
T_REV2 = 4.3


def midi(n):
    return 440.0 * 2 ** ((n - 69) / 12)


def smoothstep(x):
    x = np.clip(x, 0, 1)
    return x * x * (3 - 2 * x)


def ramp(t0, t1):
    return smoothstep((T - t0) / (t1 - t0))


# ---------------------------------------------------------------- pad ------
# Cmaj9(#11) colour (C Lydian): C, E, G, B and D, with F#.
# Chord A until 4.3 s, then voices glide to chord B (Em9-ish over C).
CHORD_A = [40, 48, 55, 59, 62, 64, 67, 71]   # E2 C3 G3 B3 D4 E4 G4 B4
CHORD_B = [40, 48, 55, 59, 64, 66, 71, 74]   # E2 C3 G3 B3 E4 F#4 B4 D5
CHORD_C = [38, 50, 57, 59, 64, 66, 69, 74]   # D2 D3 A3 B3 E4 F#4 A4 D5, slow drift at 8.4 s
T_DRIFT = 8.4
VOICE_GAIN = [0.55, 0.7, 0.8, 0.7, 0.6, 0.55, 0.42, 0.32]

# Filter cutoff curve (Hz): dark during the build, opens at reveal 1,
# a touch brighter at reveal 2, closes again during the fade.
cut = (420 + 260 * ramp(0.4, 1.6)
       + 1300 * ramp(T_REV1 - 0.03, T_REV1 + 0.12)
       - 500 * ramp(T_REV1 + 0.3, 3.2)
       + 350 * ramp(T_REV2 - 0.02, T_REV2 + 0.15)
       - 350 * ramp(T_REV2 + 0.3, 6.0)
       - 700 * ramp(9.0, 13.0))
cut = np.maximum(cut, 250)


def pad_channel(chan_seed):
    r = np.random.default_rng(SEED + chan_seed)
    out = np.zeros(N, np.float64)
    g1 = smoothstep((T - (T_REV2 + 0.05)) / 0.45)
    g2 = smoothstep((T - T_DRIFT) / 0.8)
    for a, b, c, g in zip(CHORD_A, CHORD_B, CHORD_C, VOICE_GAIN):
        f0 = midi(a + (b - a) * g1 + (c - b) * g2)
        for _ in range(2):  # two detuned unison copies per channel
            cents = r.uniform(-9, 9)
            vib = 1 + 0.0012 * np.sin(2 * np.pi * r.uniform(0.15, 0.35) * T + r.uniform(0, 6.28))
            f = f0 * 2 ** (cents / 1200) * vib
            ph = np.cumsum(2 * np.pi * f / SR) + r.uniform(0, 2 * np.pi)
            nh = int(min(28, 4200 / midi(min(a, b, c))))
            for h in range(1, nh + 1):
                fh = f * h
                # soft saw-ish spectrum through a 3-pole-style low-pass
                gain = (1.0 / h ** 1.15) / np.sqrt(1 + (fh / cut) ** 6)
                out += g * gain * np.sin(h * ph + r.uniform(0, 6.28))
    return out


# ---------------------------------------------------------- air / noise ----
def coloured_noise(seed, lo, hi, order=2):
    r = np.random.default_rng(SEED + seed)
    w = r.standard_normal(N)
    sos = signal.butter(order, [lo, hi], "bandpass", fs=SR, output="sos")
    return signal.sosfilt(sos, w)


def air_channel(k):
    breath = coloured_noise(100 + k, 300, 2500)
    hiss = coloured_noise(110 + k, 2500, 9000)
    # swell rising into reveal 1, then a low steady bed
    # the swell pulls back over the last 80 ms so the reveal lands clean
    swell = ramp(0.4, T_REV1 - 0.1) ** 1.6 * (1 - 0.8 * ramp(T_REV1 - 0.1, T_REV1 - 0.01))
    bed = 0.35 * ramp(T_REV1, 2.4)
    return breath * (0.9 * swell + bed) * 0.06 + hiss * (0.5 * swell + 0.03 * bed) * 0.02


# ------------------------------------------------------------ crackle ------
def crackle_channel(k):
    r = np.random.default_rng(SEED + 200 + k)
    out = np.zeros(N)
    t = T_REV1 + 0.1
    while t < DUR - 0.5:
        t += r.exponential(1 / 5.0)
        i = int(t * SR)
        if i >= N - 400:
            break
        amp = r.uniform(0.2, 1.0) ** 2
        L = r.integers(30, 260)
        burst = r.standard_normal(L) * np.exp(-np.arange(L) / (L / 4))
        out[i:i + L] += amp * burst
    sos = signal.butter(2, [60, 4500], "bandpass", fs=SR, output="sos")
    return signal.sosfilt(sos, out) * 0.0022


# -------------------------------------------------------------- chimes -----
def chime(t0, notes, level, k):
    """Soft bell: slightly inharmonic partials, 12 ms attack, long decay."""
    r = np.random.default_rng(SEED + 300 + k)
    out = np.zeros(N)
    i0 = int(t0 * SR)
    tt = np.arange(N - i0) / SR
    att = smoothstep(tt / 0.012)
    att = smoothstep(tt / 0.006)
    partials = [(1.0, 1.0, 2.6), (2.0, 0.4, 1.6), (3.01, 0.25, 0.9),
                (4.2, 0.18, 0.45), (5.43, 0.12, 0.25), (6.8, 0.07, 0.12)]
    for n, nv in enumerate(notes):
        f = midi(nv)
        delay = int(n * 0.018 * SR)  # tiny strum
        for ratio, a, dec in partials:
            fr = f * ratio * (1 + r.uniform(-0.0008, 0.0008))
            if fr > 9000:
                continue
            s = a * att * np.exp(-tt / dec) * np.sin(2 * np.pi * fr * tt + r.uniform(0, 6.28))
            out[i0 + delay:] += s[: len(out) - i0 - delay]
    return out * level


def bloom(t0, level, k):
    """Short swell of the pad's upper octave, peaking just after t0."""
    out = np.zeros(N)
    r = np.random.default_rng(SEED + 400 + k)
    env = np.exp(-np.maximum(T - t0, 0) / 0.9) * smoothstep((T - (t0 - 0.06)) / 0.08)
    for nv in [55, 60, 64, 67]:  # G3 C4 E4 G4
        f = midi(nv) * (1 + r.uniform(-0.002, 0.002))
        out += np.sin(2 * np.pi * f * T + r.uniform(0, 6.28)) * 0.25
        out += np.sin(2 * np.pi * 2 * f * T + r.uniform(0, 6.28)) * 0.06
    return out * env * level


def thump(t0, level, f_end=65.4):
    """Soft low thump: sine with a quick downward pitch drop, plus a dull felt tick."""
    out = np.zeros(N)
    i0 = int(t0 * SR)
    tt = np.arange(N - i0) / SR
    f = f_end * (1 + 0.9 * np.exp(-tt / 0.03))
    ph = np.cumsum(2 * np.pi * f / SR)
    env = smoothstep(tt / 0.004) * np.exp(-tt / 0.32)
    body = np.sin(ph) + 0.25 * np.sin(2 * ph) * np.exp(-tt / 0.12)
    r = np.random.default_rng(SEED + 600 + int(t0 * 10))
    tick = r.standard_normal(len(tt)) * np.exp(-tt / 0.006)
    tick = signal.sosfilt(signal.butter(2, [200, 4500], "bandpass", fs=SR, output="sos"), tick)
    out[i0:] = env * body + 0.18 * tick
    return out * level


# ------------------------------------------------------------- reverb ------
def make_ir(k, rt60=3.2, length=4.0):
    r = np.random.default_rng(SEED + 500 + k)
    n = int(length * SR)
    t = np.arange(n) / SR
    noise = r.standard_normal(n)
    lo = signal.sosfilt(signal.butter(2, 900, "low", fs=SR, output="sos"), noise)
    hi = signal.sosfilt(signal.butter(2, 900, "high", fs=SR, output="sos"), noise)
    ir = lo * 10 ** (-3 * t / rt60) + hi * 10 ** (-3 * t / (rt60 * 0.55))
    ir *= smoothstep(t / 0.03)            # soft onset, no hard early reflection
    ir = np.concatenate([np.zeros(int(0.025 * SR)), ir])
    return ir / np.sqrt(np.sum(ir ** 2))


# ---------------------------------------------------------- loudness -------
def lufs(x):
    sos = np.array([
        [1.53512485958697, -2.69169618940638, 1.19839281085285, 1, -1.69065929318241, 0.73248077421585],
        [1.0, -2.0, 1.0, 1, -1.99004745483398, 0.99007225036621]])
    y = signal.sosfilt(sos, x, axis=0)
    blk, hop = int(0.4 * SR), int(0.1 * SR)
    z = np.array([np.mean(y[i:i + blk] ** 2, axis=0).sum() for i in range(0, len(y) - blk, hop)])
    lk = -0.691 + 10 * np.log10(z + 1e-12)
    z = z[lk > -70]
    rel = -0.691 + 10 * np.log10(z.mean()) - 10
    z = z[(-0.691 + 10 * np.log10(z)) > rel]
    return -0.691 + 10 * np.log10(z.mean())


def true_peak_db(x):
    up = signal.resample_poly(x, 4, 1, axis=0)
    return 20 * np.log10(np.abs(up).max())


# ------------------------------------------------------------------ mix ----
def main(path):
    master = ramp(0.38, 0.75) * (0.25 + 0.75 * ramp(0.5, T_REV1) ** 1.3)
    master = np.where(T >= T_REV1, 1.0, master) * (1 - ramp(10.0, 13.3))
    # Pad: quieter during the build, steps up at reveal 1
    pad_lvl = (0.35 + 0.65 * ramp(T_REV1 - 0.05, T_REV1 + 0.35)) * (1 + 0.12 * ramp(T_REV2, T_REV2 + 0.5))

    chans = []
    for k in range(2):
        pad = pad_channel(k * 17) * pad_lvl * 0.05
        dry = pad + air_channel(k) + crackle_channel(k)
        # warm mid-register bell under each thump (C4 E4 G4, then E4 B4)
        acc = (chime(T_REV1, [60, 64, 67] if k == 0 else [64, 60, 67], 0.045, k)
               + bloom(T_REV1, 0.045, k)
               + chime(T_REV2, [64, 71] if k == 0 else [71, 64], 0.03, 10 + k)
               + bloom(T_REV2, 0.016, 10 + k))
        chans.append((dry * master, acc * (1 - ramp(10.0, 13.3))))

    thumps = thump(T_REV1, 0.30, 65.4) + thump(T_REV2, 0.17, 82.4)   # C2, then E2

    out = np.zeros((N, 2))
    for k, (dry, acc) in enumerate(chans):
        ir = make_ir(k)
        wet = signal.fftconvolve(dry + acc + 0.25 * thumps, ir)[:N]
        out[:, k] = 0.62 * dry + 0.7 * acc + thumps + 0.55 * wet
    # tame anything that would fall after the fade, then end silent
    out *= (1 - ramp(13.3, 13.9))[:, None]
    out = signal.sosfilt(signal.butter(2, 25, "high", fs=SR, output="sos"), out, axis=0)

    target = -17.0
    out *= 10 ** ((target - lufs(out)) / 20)
    tp = true_peak_db(out)
    if tp > -1.5:
        out *= 10 ** ((-1.5 - tp) / 20)
    print(f"LUFS {lufs(out):.2f}  true peak {true_peak_db(out):.2f} dBTP")

    pcm = np.clip(np.round(out * 32767), -32768, 32767).astype("<i2")
    from scipy.io import wavfile
    wavfile.write(path, SR, pcm)
    print("wrote", path)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "/tmp/welcome-sound/welcome-synth.wav")
