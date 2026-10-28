#pragma once
#include <algorithm>
#include <vector>

namespace qe
{
// Ringpuffer, vorab allokiert. Kein Allokieren im Audio-Thread.
class DelayLine
{
public:
    // Exakte Groesse (keine Zweierpotenz): bei 16 s und 192 kHz spart das bis zur Haelfte des Speichers
    void prepare (int minCapacity)
    {
        cap = minCapacity + 8;
        buf.assign ((size_t) cap, 0.0f);
        writePos = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    // Teil des Puffers loeschen (verteiltes Loeschen ueber mehrere Audio-Bloecke)
    void clearRange (int start, int count)
    {
        start = std::min (std::max (start, 0), capacity());
        count = std::min (std::max (count, 0), capacity() - start);
        std::fill (buf.begin() + start, buf.begin() + start + count, 0.0f);
    }
    int capacity() const { return cap; }

    void push (float x) { writePos = writePos + 1 == cap ? 0 : writePos + 1; buf[(size_t) writePos] = x; }

    // Verzoegerung d (>= 2) Samples relativ zum NAECHSTEN zu schreibenden Sample.
    // Aufruf VOR push(): ganzzahliges d liefert exakt in[n-d].
    // d in double: in float waere die Aufloesung bei 4 s / 192 kHz nur 1/16 Sample.
    float readHermite (double d) const
    {
        const int di = (int) d;
        const float t = (float) (1.0 - (d - (double) di));
        const int i1 = wrap (writePos - di);
        const int im1 = i1 == 0 ? cap - 1 : i1 - 1;
        const int ip1 = i1 + 1 == cap ? 0 : i1 + 1;
        const int ip2 = ip1 + 1 == cap ? 0 : ip1 + 1;
        const float ym1 = buf[(size_t) im1];
        const float y0  = buf[(size_t) i1];
        const float y1  = buf[(size_t) ip1];
        const float y2  = buf[(size_t) ip2];
        const float c1 = 0.5f * (y1 - ym1);
        const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
        const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
        return ((c3 * t + c2) * t + c1) * t + y0;
    }

    // Verzoegerung d (>= 0) Samples relativ zum zuletzt GESCHRIEBENEN Sample.
    // Aufruf NACH push(): d = 0 liefert das aktuelle Sample.
    float readLinear (float d) const
    {
        const int di = (int) d;
        const float frac = d - (float) di;
        const int i0 = wrap (writePos - di);
        const float a = buf[(size_t) i0];
        const float b = buf[(size_t) (i0 == 0 ? cap - 1 : i0 - 1)];
        return a + (b - a) * frac;
    }

private:
    int wrap (int i) const { return i < 0 ? i + cap : i; }   // Lese-Abstand ist immer kleiner als cap

    std::vector<float> buf;
    int cap = 1;
    int writePos = 0;
};
} // namespace qe
