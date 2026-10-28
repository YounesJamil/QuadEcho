#!/usr/bin/env python3
"""Referenzmodell einer QUAD-ECHO-DelayUnit in double (DSP-Audit, Phase 0).

Bildet DelayUnit::process (Source/DelayUnit.cpp) Schritt fuer Schritt nach: dieselbe Struktur (16er-Teilbloecke fuer
Filter-Koeffizienten und Huellkurve, Hermite-Lesen, Transitionen, Kanalmodi), aber durchgehend in double und ohne
Eingangs-Saettigung (die Dumps nutzen Input-Gain <= 0 dB, dort ist sie im Plugin ohnehin aus).
  --sat tanh    Schleifen-tanh wie im Plugin bei Drive 0: misst nur die Rechengenauigkeit (float gegen double)
  --sat linear  ideal linear: misst zusaetzlich, wie stark der tanh in der Schleife das Signal veraendert

Nicht nachgebildet (die Dumps verwenden es nicht): Freeze, Drive, Parameter-Glaettung (SmoothedValue) bei
Aenderungen waehrend des Renders. Einzige erlaubte Aenderung ist ein Sprung von timeL (Transitionen).

Eingabe: Ordner aus  QuadEchoTests --harness refdump <ordner>  (JSON + float32-Dateien, kanalweise hintereinander).
Aufruf:  python tests/ref/delayunit_ref.py <ordner> [--sat tanh|linear] [--only r03] [--out ergebnis.tsv]
"""
import argparse
import json
import math
import os
import sys
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

import numpy as np

KSUB = 16
MAX_DELAY_S = 4.0
NOTE_DIVS = [("1/32", 0.125), ("1/16T", 1.0 / 6.0), ("1/16", 0.25), ("1/16.", 0.375), ("1/8T", 1.0 / 3.0),
             ("1/8", 0.5), ("1/8.", 0.75), ("1/4T", 2.0 / 3.0), ("1/4", 1.0), ("1/4.", 1.5), ("1/2T", 4.0 / 3.0),
             ("1/2", 2.0), ("1/2.", 3.0), ("1/1T", 8.0 / 3.0), ("1/1", 4.0), ("2/1", 8.0), ("4/1", 16.0)]
STEREO, PINGPONG, MIDSIDE = 0, 1, 2
REPITCH, FADE, JUMP = 0, 1, 2
SYNC_NOTES, SYNC_TRIPLET, SYNC_DOTTED, SYNC_16TH = 0, 1, 2, 3
SINE, TRIANGLE, SAWUP, SAWDOWN, SQUARE, SMOOTHRANDOM = range(6)
M64 = (1 << 64) - 1
HALF_PI = math.pi / 2


def clamp(v, lo, hi):
    return min(max(v, lo), hi)


# ---- DspTypes.h ----------------------------------------------------------------------------------------------
def lfo_random_at(k):
    x = ((k & M64) * 0x9E3779B97F4A7C15) & M64
    x ^= x >> 31
    x = (x * 0xBF58476D1CE4E5B9) & M64
    x ^= x >> 29
    x = (x * 0x94D049BB133111EB) & M64
    x ^= x >> 32
    return (x >> 11) * (1.0 / 9007199254740992.0) * 2.0 - 1.0


def lfo_wave(wave, t):
    cycle = math.floor(t)
    p = t - cycle
    if wave == SINE:
        return math.sin(2.0 * math.pi * p)
    if wave == TRIANGLE:
        q = p + 0.25
        q -= math.floor(q)
        return 1.0 - 4.0 * abs(q - 0.5)
    if wave == SAWUP:
        return 2.0 * p - 1.0
    if wave == SAWDOWN:
        return 1.0 - 2.0 * p
    if wave == SMOOTHRANDOM:
        a, b = lfo_random_at(int(cycle)), lfo_random_at(int(cycle) + 1)
        return a + (b - a) * (0.5 - 0.5 * math.cos(math.pi * p))
    return 1.0 if p < 0.5 else -1.0


def div_beats(i):
    return NOTE_DIVS[clamp(i, 0, len(NOTE_DIVS) - 1)][1]


def note_family(i):
    last = NOTE_DIVS[clamp(i, 0, len(NOTE_DIVS) - 1)][0][-1]
    return 1 if last == "T" else (2 if last == "." else 0)


def family_of_mode(mode):
    return 1 if mode == SYNC_TRIPLET else (2 if mode == SYNC_DOTTED else 0)


def note_base_beats(i):
    b, f = div_beats(i), note_family(i)
    return b * 1.5 if f == 1 else (b / 1.5 if f == 2 else b)


def div_in_mode(div, mode):
    div = clamp(div, 0, len(NOTE_DIVS) - 1)
    fam = family_of_mode(mode)
    if note_family(div) == fam:
        return div
    best, best_dist = div, 1.0e9
    for i in range(len(NOTE_DIVS)):
        if note_family(i) == fam:
            d = abs(math.log(note_base_beats(i) / note_base_beats(div)))
            if d < best_dist:
                best_dist, best = d, i
    return best


def delay_times_ms(s, bpm):
    beat_ms = 60000.0 / max(20.0, bpm)

    def sync_beats(right):
        mode = s["syncModeR"] if right else s["syncMode"]
        if mode == SYNC_16TH:
            return 0.25 * clamp(s["sixR"] if right else s["sixL"], 1, 16)
        return div_beats(div_in_mode(s["divR"] if right else s["divL"], mode))

    ms_l = sync_beats(False) * beat_ms if s["sync"] else s["timeL"]
    ms_r = ms_l if s["link"] else (sync_beats(True) * beat_ms if s["sync"] else s["timeR"])
    ms_l *= 1.0 + 0.01 * s["offsetL"]
    ms_r *= 1.0 + 0.01 * s["offset"]
    max_ms = MAX_DELAY_S * 1000.0
    return clamp(ms_l, 1.0, max_ms), clamp(ms_r, 1.0, max_ms)


# ---- Filter (JUCE StateVariableTPTFilter, FirstOrderTPTFilter; PeakEq aus DspTypes.h) ----------------------
class Svf:
    def __init__(self, highpass):
        self.hp = highpass
        self.s1 = [0.0, 0.0]
        self.s2 = [0.0, 0.0]
        self.g = self.h = self.r2 = 0.0

    def set(self, sr, fc, q):
        self.g = math.tan(math.pi * fc / sr)
        self.r2 = 1.0 / q
        self.h = 1.0 / (1.0 + self.r2 * self.g + self.g * self.g)

    def reset(self):
        self.s1 = [0.0, 0.0]
        self.s2 = [0.0, 0.0]

    def tick(self, ch, x):
        s1, s2, g = self.s1[ch], self.s2[ch], self.g
        y_hp = self.h * (x - s1 * (g + self.r2) - s2)
        y_bp = y_hp * g + s1
        self.s1[ch] = y_hp * g + y_bp
        y_lp = y_bp * g + s2
        self.s2[ch] = y_bp * g + y_lp
        return y_hp if self.hp else y_lp


class OnePole:
    def __init__(self, highpass):
        self.hp = highpass
        self.s = [0.0, 0.0]
        self.G = 0.0

    def set(self, sr, fc):
        g = math.tan(math.pi * fc / sr)
        self.G = g / (1.0 + g)

    def reset(self):
        self.s = [0.0, 0.0]

    def tick(self, ch, x):
        s = self.s[ch]
        v = self.G * (x - s)
        y = v + s
        self.s[ch] = y + v
        return x - y if self.hp else y


class PeakEq:
    def __init__(self):
        self.b0, self.b1, self.b2, self.a1, self.a2 = 1.0, 0.0, 0.0, 0.0, 0.0
        self.z1 = [0.0, 0.0]
        self.z2 = [0.0, 0.0]

    def set(self, sr, freq, q, gain_db):
        A = 10.0 ** (gain_db / 40.0)
        w0 = 2.0 * math.pi * min(freq, 0.45 * sr) / sr
        alpha = math.sin(w0) / (2.0 * max(q, 0.1))
        cw, a0 = math.cos(w0), 1.0 + alpha / A
        self.b0 = (1.0 + alpha * A) / a0
        self.b1 = -2.0 * cw / a0
        self.b2 = (1.0 - alpha * A) / a0
        self.a1 = self.b1
        self.a2 = (1.0 - alpha / A) / a0

    def reset(self):
        self.z1 = [0.0, 0.0]
        self.z2 = [0.0, 0.0]

    def tick(self, ch, x):
        y = self.b0 * x + self.z1[ch]
        self.z1[ch] = self.b1 * x - self.a1 * y + self.z2[ch]
        self.z2[ch] = self.b2 * x - self.a2 * y
        return y


# ---- DelayLine.h + Line aus DelayUnit.h ------------------------------------------------------------------------
class Line:
    def __init__(self, min_capacity):
        c = 1
        while c < min_capacity + 8:
            c <<= 1
        self.buf = [0.0] * c
        self.mask = c - 1
        self.wp = 0
        self.base_a = self.base_b = self.cur = 0.0
        self.fading = self.init = False
        self.fade_pos, self.fade_len = 0, 1

    def push(self, x):
        self.wp = (self.wp + 1) & self.mask
        self.buf[self.wp] = x

    def read_hermite(self, d):
        di = int(d)
        t = 1.0 - (d - di)
        i1 = self.wp - di
        b, m = self.buf, self.mask
        ym1, y0, y1, y2 = b[(i1 - 1) & m], b[i1 & m], b[(i1 + 1) & m], b[(i1 + 2) & m]
        c1 = 0.5 * (y1 - ym1)
        c2 = ym1 - 2.5 * y0 + 2.0 * y1 - 0.5 * y2
        c3 = 0.5 * (y2 - ym1) + 1.5 * (y0 - y1)
        return ((c3 * t + c2) * t + c1) * t + y0


# ---- DelayUnit ------------------------------------------------------------------------------------------------
class DelayUnitModel:
    def __init__(self, sr, sat="tanh"):
        self.sr = sr
        cap = math.ceil(MAX_DELAY_S * sr) + math.ceil(0.05 * sr) + 16
        self.lines = [Line(cap), Line(cap)]
        self.capacity = self.lines[0].mask + 1
        self.hp, self.lp = Svf(True), Svf(False)
        self.hp1, self.lp1 = OnePole(True), OnePole(False)
        self.peak_hp, self.peak_lp = PeakEq(), PeakEq()
        self.rp_coef = 1.0 - math.exp(-1.0 / (0.2 * sr))
        self.q_coef = 1.0 - math.exp(-KSUB / (0.03 * sr))
        self.mod_coef = 1.0 - math.exp(-1.0 / (0.004 * sr))
        self.env_atk = 1.0 - math.exp(-1.0 / (0.005 * sr))
        self.env_rel = 1.0 - math.exp(-1.0 / (0.15 * sr))
        self.lfo_pos = 0.0
        self.mod_sm = [0.0, 0.0]
        self.f_mod_sm = 0.0
        self.env = self.env_mapped = 0.0
        self.q_sm = [0.8, 0.8]
        self.boost_sm = [0.0, 0.0]
        self.last_transition = -1
        self.initialised = False
        self.loop_sat = math.tanh if sat == "tanh" else (lambda x: x)

    def read_line(self, l, mod, target, transition):
        max_d = float(self.capacity - 8)

        def rd(d):
            return l.read_hermite(min(max(d, 2.0), max_d))

        if not l.init:
            l.base_a = l.base_b = l.cur = target
            l.init = True
        if transition == REPITCH:
            l.cur += (target - l.cur) * self.rp_coef
            l.base_a = l.cur
            l.fading = False
            return rd(l.base_a + mod)
        if not l.fading and abs(target - l.base_a) > 0.5:
            l.fading = True
            l.fade_pos = 0
            l.fade_len = max(1, int(self.sr * (0.06 if transition == FADE else 0.003)))
            l.base_b = target
        if not l.fading:
            return rd(l.base_a + mod)
        g = l.fade_pos / l.fade_len
        ga, gb = math.cos(g * HALF_PI), math.sin(g * HALF_PI)
        ya, yb = rd(l.base_a + mod), rd(l.base_b + mod)
        l.fade_pos += 1
        if l.fade_pos >= l.fade_len:
            l.base_a = l.base_b
            l.fading = False
        return ga * ya + gb * yb

    def process(self, in_l, in_r, wet_l, wet_r, start_idx, n, s, bpm):
        sr = self.sr
        assert not s["freeze"] and s["inGainDb"] <= 0.0, "Freeze und Drive sind nicht nachgebildet"
        trans = clamp(s["transition"], 0, 2)
        if trans != self.last_transition:
            for l in self.lines:
                if l.fading:
                    l.base_a = l.base_b
                l.fading = False
                l.cur = l.base_a
            self.last_transition = trans

        ms_l, ms_r = delay_times_ms(s, bpm)
        target = (ms_l * sr / 1000.0, ms_r * sr / 1000.0)
        lfo_hz = (bpm / 60.0) / div_beats(s["lfoDiv"]) if s["lfoSync"] else s["rateHz"]
        lfo_inc = lfo_hz / sr
        phase_off = s["phaseDeg"] / 360.0
        f_phase_off = s["filterPhaseDeg"] / 360.0
        gate = 1.0 if s["lfoOn"] else 0.0
        depth = s["delayDepth"] * 0.010 * (4.0 if s["x4"] else 1.0) * sr
        fb_sign = -1.0 if s["fbInvert"] else 1.0
        q_lp_t = 0.7071 + s["res"] * s["res"] * 4.0
        q_hp_t = 0.7071 + s["hpRes"] * s["hpRes"] * 4.0
        mode = s["mode"]
        if not self.initialised:
            self.q_sm = [q_hp_t, q_lp_t]
            self.boost_sm = [s["hpBoost"], s["lpBoost"]]
            self.initialised = True
        # konstante Parameter: die Glaettung steht auf dem Zielwert
        in_g = 10.0 ** (s["inGainDb"] * 0.05)
        fb = s["feedback"] * fb_sign
        out_g = 0.0 if s["mute"] else 10.0 ** (s["outDb"] * 0.05)
        wid = s["width"]
        lvl = s["mix"]
        f_depth = s["filterDepth"]
        env_mix = s["envMix"]
        wave = s["wave"]
        hp_on, lp_on = s["hpOn"], s["lpOn"]
        hp, lp, hp1, lp1, pk_hp, pk_lp = self.hp, self.lp, self.hp1, self.lp1, self.peak_hp, self.peak_lp
        line0, line1 = self.lines
        loop_sat = self.loop_sat

        for sub in range(0, n, KSUB):
            ln = min(KSUB, n - sub)
            env_db = 20.0 * math.log10(self.env + 1e-6)
            self.env_mapped = clamp((env_db + 48.0) / 48.0, 0.0, 1.0)
            self.q_sm[0] += (q_hp_t - self.q_sm[0]) * self.q_coef
            self.q_sm[1] += (q_lp_t - self.q_sm[1]) * self.q_coef
            q_hp, q_lp = self.q_sm
            self.boost_sm[0] += (s["hpBoost"] - self.boost_sm[0]) * self.q_coef
            self.boost_sm[1] += (s["lpBoost"] - self.boost_sm[1]) * self.q_coef
            hp_peak = hp_on and self.boost_sm[0] > 0.05
            lp_peak = lp_on and self.boost_sm[1] > 0.05
            mod_f = clamp(self.f_mod_sm, -1.0, 1.0)
            fm = 2.0 ** (mod_f * f_depth * 2.0)
            max_f = 0.45 * sr
            if hp_on:
                fc = clamp(s["hpHz"] * fm, 20.0, max_f)
                hp.set(sr, fc, q_hp)
                hp1.set(sr, fc)
                if hp_peak:
                    pk_hp.set(sr, fc, q_hp, self.boost_sm[0])
                else:
                    pk_hp.reset()
            else:
                hp.reset(); hp1.reset(); pk_hp.reset()
            if lp_on:
                fc = clamp(s["lpHz"] * fm, 100.0, max_f)
                lp.set(sr, fc, q_lp)
                lp1.set(sr, fc)
                if lp_peak:
                    pk_lp.set(sr, fc, q_lp, self.boost_sm[1])
                else:
                    pk_lp.reset()
            else:
                lp.reset(); lp1.reset(); pk_lp.reset()
            env_mapped = self.env_mapped

            for j in range(ln):
                i = start_idx + sub + j
                l, r = in_l[i], in_r[i]
                a = max(abs(l), abs(r))
                self.env += (self.env_atk if a > self.env else self.env_rel) * (a - self.env)

                lp_ = self.lfo_pos
                lfo0 = gate * lfo_wave(wave, lp_)
                lfo1 = gate * lfo_wave(wave, lp_ + phase_off)
                lfo_f = gate * lfo_wave(wave, lp_ + f_phase_off)
                self.lfo_pos = lp_ + lfo_inc
                m0 = lfo0 * (1.0 - env_mix) + env_mapped * env_mix
                m1 = lfo1 * (1.0 - env_mix) + env_mapped * env_mix
                mf = lfo_f * (1.0 - env_mix) + env_mapped * env_mix
                self.f_mod_sm += (mf - self.f_mod_sm) * self.mod_coef
                self.mod_sm[0] += (m0 - self.mod_sm[0]) * self.mod_coef
                self.mod_sm[1] += (m1 - self.mod_sm[1]) * self.mod_coef

                if mode == PINGPONG:
                    x0, x1 = 0.5 * (l + r) * in_g, 0.0
                elif mode == MIDSIDE:
                    x0, x1 = 0.5 * (l + r) * in_g, 0.5 * (l - r) * in_g
                else:
                    x0, x1 = l * in_g, r * in_g

                y0 = self.read_line(line0, self.mod_sm[0] * depth, target[0], trans)
                y1 = self.read_line(line1, self.mod_sm[1] * depth, target[1], trans)

                r0, r1 = (y1, y0) if mode == PINGPONG else (y0, y1)
                f0, f1 = r0, r1
                if hp_on:
                    f0 = hp1.tick(0, hp.tick(0, f0)); f1 = hp1.tick(1, hp.tick(1, f1))
                if lp_on:
                    f0 = lp1.tick(0, lp.tick(0, f0)); f1 = lp1.tick(1, lp.tick(1, f1))
                if hp_peak:
                    f0 = pk_hp.tick(0, f0); f1 = pk_hp.tick(1, f1)
                if lp_peak:
                    f0 = pk_lp.tick(0, f0); f1 = pk_lp.tick(1, f1)
                f0, f1 = loop_sat(f0), loop_sat(f1)
                w0, w1 = x0 + fb * f0, x1 + fb * f1
                if abs(w0) < 1e-18:
                    w0 = 0.0
                if abs(w1) < 1e-18:
                    w1 = 0.0
                line0.push(w0)
                line1.push(w1)

                if mode == MIDSIDE:
                    wl, wr = y0 + y1, y0 - y1
                else:
                    wl, wr = y0, y1
                mid, side = 0.5 * (wl + wr), 0.5 * (wl - wr) * wid
                wet_l[i] = (mid + side) * lvl * out_g
                wet_r[i] = (mid - side) * lvl * out_g


def run_case(json_path, sat):
    meta = json.loads(Path(json_path).read_text())
    base = str(json_path)[:-5]
    n, sr, block, bpm = meta["n"], meta["sr"], meta["block"], meta["bpm"]
    x = np.fromfile(base + "_in.f32", dtype=np.float32).astype(np.float64)
    c = np.fromfile(base + "_wet.f32", dtype=np.float32).astype(np.float64)
    in_l, in_r = x[:n].tolist(), x[n:].tolist()
    wet_l, wet_r = [0.0] * n, [0.0] * n
    unit = DelayUnitModel(sr, sat)
    s = dict(meta["settings"])
    s.setdefault("mute", False)
    changes = sorted(meta.get("changes", []), key=lambda ch: ch["at"])
    for pos in range(0, n, block):
        for ch in changes:
            if pos >= ch["at"]:
                s[ch["field"]] = ch["value"]
        unit.process(in_l, in_r, wet_l, wet_r, pos, min(block, n - pos), s, bpm)
    ref = np.concatenate([np.array(wet_l), np.array(wet_r)])
    d = c - ref
    peak_d = float(np.max(np.abs(d)))
    rms_d = float(np.sqrt(np.mean(d * d)))
    rms_ref = float(np.sqrt(np.mean(ref * ref)))
    k = int(np.argmax(np.abs(d)))
    return {
        "name": meta["name"], "sr": int(sr), "sat": sat,
        "peak_diff_dbfs": 20 * math.log10(peak_d) if peak_d > 0 else -math.inf,
        "rms_diff_dbfs": 20 * math.log10(rms_d) if rms_d > 0 else -math.inf,
        "signal_rms_dbfs": 20 * math.log10(rms_ref) if rms_ref > 0 else -math.inf,
        "worst_at_s": (k % n) / sr, "worst_ch": "L" if k < n else "R",
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("dir")
    ap.add_argument("--sat", choices=["tanh", "linear", "both"], default="both")
    ap.add_argument("--only", default="")
    ap.add_argument("--out", default="")
    a = ap.parse_args()
    files = sorted(p for p in Path(a.dir).glob("*.json") if a.only in p.name)
    sats = ["tanh", "linear"] if a.sat == "both" else [a.sat]
    jobs = [(str(f), s) for s in sats for f in files]
    with ProcessPoolExecutor(max_workers=max(1, (os.cpu_count() or 2) - 2)) as ex:
        results = list(ex.map(run_case, *zip(*jobs)))
    results.sort(key=lambda r: (r["sat"], r["name"], r["sr"]))
    lines = ["case\tsr\tsat\tpeak_diff_dBFS\trms_diff_dBFS\tsignal_rms_dBFS\tSNR_dB\tworst_at_s"]
    for r in results:
        snr = r["signal_rms_dbfs"] - r["rms_diff_dbfs"]
        lines.append(f'{r["name"]}\t{r["sr"]}\t{r["sat"]}\t{r["peak_diff_dbfs"]:.1f}\t{r["rms_diff_dbfs"]:.1f}\t'
                     f'{r["signal_rms_dbfs"]:.1f}\t{snr:.1f}\t{r["worst_at_s"]:.4f}{r["worst_ch"]}')
    text = "\n".join(lines) + "\n"
    sys.stdout.write(text)
    if a.out:
        Path(a.out).write_text(text)


if __name__ == "__main__":
    main()
