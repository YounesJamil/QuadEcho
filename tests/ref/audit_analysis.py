#!/usr/bin/env python3
"""Auswertungen fuer das DSP-Audit (Phase 1): Schleifenverstaerkung, Hermite-Frequenzgang, Aliasing der Schleifen-Saettigung.

Analytisch: Frequenzgaenge der Filter in der Feedback-Schleife (JUCE TPT-SVF 2. Ordnung + TPT 1. Ordnung = bilineare
Transformation mit Vorverzerrung auf fc; Glocke nach RBJ) und des Hermite-Interpolators (4-Punkt, 3. Ordnung).
Gemessen: Dumps aus  QuadEchoTests --harness audit dumps <ordner>.
Aufruf:  python tests/ref/audit_analysis.py <dump-ordner>
"""
import json
import math
import sys
from pathlib import Path

import numpy as np


def q_of(res):
    return 0.7071 + 4.0 * res * res


def loop_filter_response(f, sr, hp_hz, lp_hz, q_hp, q_lp, hp_boost, lp_boost, hp_on=True, lp_on=True):
    """Betrag |H| der Filterkette in der Schleife (HP2*HP1, LP2*LP1, Glocken) bei den Frequenzen f."""
    w = 2 * np.pi * f / sr
    z = np.exp(1j * w)
    h = np.ones_like(f, dtype=complex)

    def tpt(fc, q, highpass):
        v = np.tan(w / 2) / math.tan(math.pi * fc / sr)
        den2 = 1 - v * v + 1j * v / q
        h2 = (-v * v / den2) if highpass else (1 / den2)
        h1 = (1j * v / (1 + 1j * v)) if highpass else (1 / (1 + 1j * v))
        return h2 * h1

    def peak(fc, q, gain_db):
        A = 10 ** (gain_db / 40)
        w0 = 2 * math.pi * min(fc, 0.45 * sr) / sr
        alpha = math.sin(w0) / (2 * max(q, 0.1))
        cw, a0 = math.cos(w0), 1 + alpha / A
        b = np.array([1 + alpha * A, -2 * cw, 1 - alpha * A]) / a0
        a = np.array([1, -2 * cw / a0, (1 - alpha / A) / a0])
        return (b[0] + b[1] / z + b[2] / z ** 2) / (a[0] + a[1] / z + a[2] / z ** 2)

    if hp_on:
        h *= tpt(hp_hz, q_hp, True)
        if hp_boost > 0.05:
            h *= peak(hp_hz, q_hp, hp_boost)
    if lp_on:
        h *= tpt(lp_hz, q_lp, False)
        if lp_boost > 0.05:
            h *= peak(lp_hz, q_lp, lp_boost)
    return np.abs(h)


def hermite_taps(frac):
    """Koeffizienten fuer ym1, y0, y1, y2 (DelayLine::readHermite, t = 1 - frac)."""
    t = 1.0 - frac
    return np.array([-0.5 * t + t * t - 0.5 * t ** 3,
                     1 - 2.5 * t * t + 1.5 * t ** 3,
                     0.5 * t + 2 * t * t - 1.5 * t ** 3,
                     -0.5 * t * t + 0.5 * t ** 3])


def hermite_mag(frac, f, sr):
    w = 2 * np.pi * np.asarray(f) / sr
    k = np.arange(4)
    return np.abs(np.exp(-1j * np.outer(w, k)) @ hermite_taps(frac))


def load(dump_dir, name):
    meta = json.loads((Path(dump_dir) / f"{name}.json").read_text())
    n = meta["n"]
    wet = np.fromfile(Path(dump_dir) / f"{name}_wet.f32", dtype=np.float32).astype(np.float64)
    return meta, wet[:n]


def echo_ratio(meta, wet):
    """|Echo2| / |Echo1| aus aufeinanderfolgenden Fenstern der Laenge d (Impulsantwort)."""
    d = meta["delaySamples"]
    L = int(round(d))
    s1 = int(round(d)) - 64
    s2 = int(round(2 * d)) - 64
    e1 = np.fft.rfft(wet[s1:s1 + L])
    e2 = np.fft.rfft(wet[s2:s2 + L])
    f = np.fft.rfftfreq(L, 1 / meta["sr"])
    return f, np.abs(e2) / np.maximum(np.abs(e1), 1e-30)


def db(x):
    return 20 * np.log10(np.maximum(x, 1e-300))


def section_loop_gain(dump_dir):
    print("## Schleifenverstaerkung |fb * H_Filter| (HP 150 Hz + LP 2500 Hz, Resonanz und Boost fuer beide gleich)")
    print("Maximum ueber 20 Hz .. 0.45 fs. Gemessen = Echo2/Echo1 bei Feedback 0.5, auf Feedback 1 umgerechnet.")
    print("res\tboost_dB\tQ\tsr\tmax|H|_analytisch_dB\tbei_Hz\tmax|H|_gemessen_dB\tAbw_dB\tSchleife_fb0.5_dB\tSchleife_fb1.1_dB")
    for sr in (48000, 192000):
        for res in (0.0, 0.5, 1.0):
            for boost in (0, 6, 12):
                name = f"loop_res{res:.1f}_boost{boost}_{sr}"
                meta, wet = load(dump_dir, name)
                f, r = echo_ratio(meta, wet)
                band = (f >= 20) & (f <= 0.45 * sr)
                meas = r[band] / 0.5
                q = q_of(res)
                ana = loop_filter_response(f[band], sr, 150, 2500, q, q, boost, boost)
                k = int(np.argmax(ana))
                dev = float(np.max(np.abs(db(meas) - db(ana))[ana > 1e-3]))
                print(f"{res}\t{boost}\t{q:.2f}\t{sr}\t{db(ana[k]):.1f}\t{f[band][k]:.0f}\t{db(np.max(meas)):.1f}\t{dev:.2f}"
                      f"\t{db(0.5 * ana[k]):+.1f}\t{db(1.1 * ana[k]):+.1f}")
    for sr in (48000, 192000):
        meta, wet = load(dump_dir, f"loop_lp12k_res1.0_boost18_{sr}")
        f, r = echo_ratio(meta, wet)
        band = (f >= 20) & (f <= 0.45 * sr)
        ana = loop_filter_response(f[band], sr, 150, 12000, 1, q_of(1.0), 0, 18, hp_on=False)
        k = int(np.argmax(ana))
        print(f"Nur LP 12 kHz, Resonanz 1, Boost 18 dB, {sr} Hz: max |H| analytisch {db(ana[k]):.1f} dB bei {f[band][k]:.0f} Hz, "
              f"gemessen {db(np.max(r[band] / 0.5)):.1f} dB; Schleife bei Feedback 1.1: {db(1.1 * ana[k]):+.1f} dB")
    print()


def section_hermite(dump_dir):
    print("## Hermite-Interpolator: Betrag je Nachkommaanteil (analytisch), dB")
    freqs = [1000, 5000, 10000, 15000, 20000]
    sr = 48000
    print("frac\t" + "\t".join(f"{f // 1000}kHz" for f in freqs) + "\t(48 kHz)")
    for frac in (0.0, 0.1, 0.2, 0.3, 0.4, 0.5):
        m = hermite_mag(frac, freqs, sr)
        print(f"{frac}\t" + "\t".join(f"{v:.2f}" for v in db(m)))
    print("\nKumuliert ueber N Wiederholungen bei frac 0.5 (48 kHz), dB:")
    print("N\t" + "\t".join(f"{f // 1000}kHz" for f in freqs))
    m = db(hermite_mag(0.5, freqs, sr))
    for n in (1, 5, 10, 20, 50):
        print(f"{n}\t" + "\t".join(f"{n * v:.1f}" for v in m))
    print("\nGemessen (Echo2/Echo1 / Feedback) gegen analytisch, 48 kHz:")
    for frac in (0.0, 0.25, 0.5):
        meta, wet = load(dump_dir, f"hermite_frac{frac:.2f}_{sr}")
        f, r = echo_ratio(meta, wet)
        real_frac = meta["delaySamples"] - math.floor(meta["delaySamples"])
        sel = [int(np.argmin(np.abs(f - x))) for x in freqs]
        meas = db(r[sel] / 0.5)
        ana = db(hermite_mag(real_frac, f[sel], sr))
        print(f"frac {real_frac:.3f}: gemessen " + " ".join(f"{v:.2f}" for v in meas) + " | analytisch " + " ".join(f"{v:.2f}" for v in ana))
    print()


def section_alias(dump_dir):
    print("## Aliasing der Schleifen-Saettigung: 7-kHz-Sinus, Feedback 0.9, je Echo k (dB relativ zum Grundton)")
    print("Harmonische = Obertoene unter fs/2; Alias = an fs/2 zurueckgefaltete Obertoene (unharmonisch).")
    for sr in (48000, 96000):
        for amp in (0.1, 0.5):
            for drive in (0, 24):
                name = f"alias_amp{amp:.1f}_drive{drive}_{sr}"
                meta, wet = load(dump_dir, name)
                d = int(round(meta["delaySamples"]))
                f0 = 7000.0
                L = int(0.2 * sr)
                win = np.blackman(L)
                rows = []
                for k in range(1, 8):
                    s = k * d + int(0.025 * sr)
                    if s + L > len(wet):
                        break
                    spec = np.abs(np.fft.rfft(wet[s:s + L] * win))
                    freqs = np.fft.rfftfreq(L, 1 / sr)

                    def power_at(fx):
                        i = int(round(fx * L / sr))
                        return float(np.sum(spec[max(i - 4, 0):i + 5] ** 2))

                    p0 = power_at(f0)
                    harm = alias = 0.0
                    seen = set()
                    for h in range(2, 40):
                        fh = h * f0
                        folded = abs(((fh + sr / 2) % sr) - sr / 2)
                        key = int(round(folded / 50))
                        if folded < 100 or key in seen or abs(folded - f0) < 200:
                            continue
                        seen.add(key)
                        if fh < sr / 2:
                            harm += power_at(folded)
                        else:
                            alias += power_at(folded)
                    rows.append((k, 10 * math.log10(max(harm, 1e-30) / p0), 10 * math.log10(max(alias, 1e-30) / p0),
                                 20 * math.log10(math.sqrt(p0) / (np.sum(win) / 2))))
                print(f"{sr} Hz, Amplitude {amp}, Drive {drive} dB: " +
                      "; ".join(f"Echo {k}: Pegel {lvl:.1f} dBFS, Harm. {h:.1f}, Alias {a:.1f}" for k, h, a, lvl in rows))
    print()


def main():
    dump_dir = sys.argv[1]
    section_loop_gain(dump_dir)
    section_hermite(dump_dir)
    section_alias(dump_dir)


if __name__ == "__main__":
    main()
