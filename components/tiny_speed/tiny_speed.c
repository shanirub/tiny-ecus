#include "tiny_speed.h"

#define MS_PER_MIN  60000
#define MILLI       1000

int32_t tiny_speed_rpm_milli(int32_t counts, uint32_t period_ms, uint32_t counts_per_turn)
{
    // rpm = counts / counts_per_turn turns, per period_ms / 60000 minutes.
    // In 64 bits: |num| <= 2^31 * 6e7 < 2^57, so neither it nor num + den / 2
    // below can overflow.
    uint64_t den = (uint64_t)period_ms * counts_per_turn;
    if (den == 0 || den > INT64_MAX) {
        return 0;  // above INT64_MAX, |num| < den / 2: the result rounds to 0
    }
    int64_t num = (int64_t)counts * MS_PER_MIN * MILLI;
    int64_t d = (int64_t)den;
    // C division truncates toward zero; adding half the divisor away from
    // zero first makes it round to nearest.
    int64_t q = (num + (num < 0 ? -d : d) / 2) / d;
    if (q > INT32_MAX) {
        return INT32_MAX;
    }
    if (q < INT32_MIN) {
        return INT32_MIN;
    }
    return (int32_t)q;
}
