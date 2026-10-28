#pragma once
#include <algorithm>
#include <vector>

namespace qe
{
// Ringpuffer, vorab allokiert. Kein Allokieren im Audio-Thread.
class DelayLine
{
public:
    void prepare (int minCapacity)
    {
        int c = 1;
        while (c < minCapacity + 8) c <<= 1;
        buf.assign ((size_t) c, 0.0f);
        mask = c - 1;
        writePos = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    int capacity() const { return mask + 1; }

    void push (float x) { writePos = (writePos + 1) & mask; buf[(size_t) writePos] = x; }

    // Verzoegerung d (>= 2) Samples relativ zum NAECHSTEN zu schreibenden Sample.
    // Aufruf VOR push(): ganzzahliges d liefert exakt in[n-d].
    float readHermite (float d) const
    {
        const int di = (int) d;
        const float t = 1.0f - (d - (float) di);
        const int i1 = writePos - di;
        const float ym1 = buf[(size_t) ((i1 - 1) & mask)];
        const float y0  = buf[(size_t) (i1 & mask)];
        const float y1  = buf[(size_t) ((i1 + 1) & mask)];
        const float y2  = buf[(size_t) ((i1 + 2) & mask)];
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
        const int i0 = writePos - di;
        const float a = buf[(size_t) (i0 & mask)];
        const float b = buf[(size_t) ((i0 - 1) & mask)];
        return a + (b - a) * frac;
    }

private:
    std::vector<float> buf;
    int mask = 0;
    int writePos = 0;
};
} // namespace qe
