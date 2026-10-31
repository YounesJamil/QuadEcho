"""Erzeugt Resources/Hrtf/kemar_df_mp64.bin aus dem MIT-KEMAR-Kompakt-Datensatz.

Quelle: HRTF Measurements of a KEMAR Dummy-Head Microphone, Bill Gardner und Keith Martin, MIT Media Lab 1994
(http://sound.media.mit.edu/resources/KEMAR.html). "This data is Copyright 1994 by the MIT Media Laboratory. It is
provided free with no restrictions on use, provided the authors are cited when the data is used in any research or
commercial application."

Aufruf: python tools/make_hrtf.py <entpackter compact-Ordner mit elev*/> <ausgabe.bin>

Verarbeitung:
- Kompakt-Set (44,1 kHz, 128 Taps, rechte Haelfte 0..180 Grad); links durch Tauschen der Ohren.
- Diffusfeld-Entzerrung: der mittlere Betragsgang aller Richtungen (beide Ohren, nach Raumwinkel gewichtet, in
  Dritteloktaven geglaettet) wird herausgerechnet (begrenzt auf +-15 dB, unter 150 Hz und ueber 15 kHz eingefroren).
  Uebrig bleiben die Richtungsunterschiede, ohne Ohrkanal-Resonanz und Messsystem.
- Raster: Hoehe -40..90 in 10-Grad-Schritten (14), Azimut 0..355 in 5-Grad-Schritten (72, 0 = vorne, 90 = rechts).
  Zwischen gemessenen Azimuten wird der logarithmische Betrag interpoliert.
- Minimalphasig, 64 Taps (1,45 ms) mit kurzem Ausblenden: die Laufzeitdifferenz liefert das Plugin selbst.
- Normiert: vorne (0, 0) hat je Ohr die Energie 1.
- Ausgabe float32 little-endian, Anordnung [hoehe][azimut][ohr 0 = links, 1 = rechts][tap].
"""
import os
import sys
import wave

import numpy as np

FS = 44100.0
NFFT = 512
TAPS = 64
ELEVS = list(range(-40, 91, 10))
AZS = list(range(0, 360, 5))


def load(root, el, az):
    w = wave.open(os.path.join(root, 'elev%d' % el, 'H%de%03da.wav' % (el, az)))
    d = np.frombuffer(w.readframes(w.getnframes()), dtype='<i2').astype(np.float64) / 32768.0
    return d[0::2], d[1::2]


def measured(root):
    """{hoehe: {azimut 0..359: (links, rechts)}} ueber den ganzen Kreis"""
    out = {}
    for el in ELEVS:
        ring = {}
        for name in sorted(os.listdir(os.path.join(root, 'elev%d' % el))):
            az = int(name.split('e')[1][:3])
            l, r = load(root, el, az)
            ring[az] = (l, r)
            if 0 < az < 180:
                ring[360 - az] = (r, l)   # Spiegelbild: Ohren tauschen
        out[el] = ring
    return out


def mags(h):
    return np.abs(np.fft.rfft(h, NFFT))


def third_octave_smooth(p, freqs):
    out = np.empty_like(p)
    for i, f in enumerate(freqs):
        if f <= 0:
            out[i] = p[i]
            continue
        lo, hi = f / 2 ** (1 / 6), f * 2 ** (1 / 6)
        sel = (freqs >= lo) & (freqs <= hi)
        out[i] = p[sel].mean()
    return out


def minphase(mag):
    full = np.concatenate([mag, mag[-2:0:-1]])
    c = np.fft.ifft(np.log(np.maximum(full, 1e-6))).real
    n = len(full)
    w = np.zeros(n)
    w[0] = 1
    w[1:n // 2] = 2
    w[n // 2] = 1
    return np.fft.ifft(np.exp(np.fft.fft(c * w))).real


def main():
    root, out_file = sys.argv[1], sys.argv[2]
    data = measured(root)
    freqs = np.fft.rfftfreq(NFFT, 1.0 / FS)

    # Diffusfeld: mittlere Leistung aller Richtungen, nach Raumwinkel (cos der Hoehe) und Ringdichte gewichtet
    acc = np.zeros(len(freqs))
    wsum = 0.0
    for el, ring in data.items():
        wel = max(np.cos(np.radians(el)), 0.05) / len(ring)
        for l, r in ring.values():
            acc += wel * (mags(l) ** 2 + mags(r) ** 2)
            wsum += 2 * wel
    df = np.sqrt(third_octave_smooth(acc / wsum, freqs))
    eq = 1.0 / np.maximum(df, 1e-6)
    i150, i15k = np.searchsorted(freqs, 150.0), np.searchsorted(freqs, 15000.0)
    eq[:i150] = eq[i150]
    eq[i15k:] = eq[i15k]
    eq_db = np.clip(20 * np.log10(eq), -15, 15)
    eq = 10 ** ((eq_db - eq_db[np.searchsorted(freqs, 1000.0)]) / 20)   # bei 1 kHz 0 dB

    fade = np.ones(TAPS)
    fade[-16:] = 0.5 + 0.5 * np.cos(np.linspace(0, np.pi, 16))

    grid = np.zeros((len(ELEVS), len(AZS), 2, TAPS))
    for ei, el in enumerate(ELEVS):
        ring = data[el]
        keys = sorted(ring.keys())
        for ai, az in enumerate(AZS):
            if len(keys) == 1:
                lm = [np.log(mags(h)) for h in ring[keys[0]]]
            else:
                # Nachbarn auf dem Kreis, Interpolation des logarithmischen Betrags
                k1 = max([k for k in keys if k <= az], default=keys[-1] - 360)
                k2 = min([k for k in keys if k > az], default=keys[0] + 360)
                t = (az - k1) / float(k2 - k1)
                h1, h2 = ring[k1 % 360], ring[k2 % 360]
                lm = [(1 - t) * np.log(mags(h1[e]) + 1e-9) + t * np.log(mags(h2[e]) + 1e-9) for e in range(2)]
            for e in range(2):
                ir = minphase(np.exp(lm[e]) * eq)[:TAPS] * fade
                grid[ei, ai, e] = ir

    front = grid[ELEVS.index(0), 0]
    norm = 1.0 / np.sqrt((front[0] ** 2).sum())
    grid *= norm
    os.makedirs(os.path.dirname(out_file), exist_ok=True)
    grid.astype('<f4').tofile(out_file)
    print('geschrieben:', out_file, grid.shape, 'Bytes', grid.size * 4)
    for el, az in [(0, 0), (0, 90), (0, 180), (0, 270), (60, 0), (-40, 0)]:
        g = grid[ELEVS.index(el), AZS.index(az)]
        print('el %3d az %3d: Energie L %.2f R %.2f' % (el, az, (g[0] ** 2).sum(), (g[1] ** 2).sum()))


if __name__ == '__main__':
    main()
