//
// Created by pei on 11/07/22.
//

#include "fdlibm.h"
//#if !__OBSOLETE_MATH

#include <stdint.h>
#include <math.h>
#include "math_config.h"
#include "sincosf.h"

/* Fast sincosf implementation.  Worst-case ULP is 0.5607, maximum relative
   error is 0.5303 * 2^-23.  A single-step range reduction is used for
   small values.  Large inputs have their range reduced using fast integer
   arithmetic.  */
void
libc_sincosf (float y, float *sinp, float *cosp)
{
    double x = y;
    double s;
    int n;
    const sincos_t *p = &__sincosf_table[0];

    if (abstop12 (y) < abstop12 (pio4))
    {
        double x2 = x * x;

        if (unlikely (abstop12 (y) < abstop12 (0x1p-12f)))
        {
            if (unlikely (abstop12 (y) < abstop12 (0x1p-126f)))
                /* Force underflow for tiny y.  */
                force_eval_float (x2);
            *sinp = y;
            *cosp = 1.0f;
            return;
        }

        sincosf_poly (x, x2, p, 0, sinp, cosp);
    }
    // [0, 5]                  [6, 10]
    else if (abstop12 (y) < abstop12 (120.0f))
    {
        x = reduce_fast (x, p, &n);

        /* Setup the signs for sin and cos.  */
        s = p->sign[n & 3];

        if (n & 2)
            p = &__sincosf_table[1];

        sincosf_poly (x * s, x * x, p, n, sinp, cosp);
    }
    else if (likely (abstop12 (y) < abstop12 (INFINITY)))
    {
        uint32_t xi = asuint (y);
        int sign = xi >> 31;

        x = reduce_large (xi, &n);

        /* Setup signs for sin and cos - include original sign.  */
        s = p->sign[(n + sign) & 3];

        if ((n + sign) & 2)
            p = &__sincosf_table[1];

        sincosf_poly (x * s, x * x, p, n, sinp, cosp);
    }
    else
    {
        /* Return NaN if Inf or NaN for both sin and cos.  */
        *sinp = *cosp = y - y;
#if WANT_ERRNO
        /* Needed to set errno for +-Inf, the add is a hack to work
       around a gcc register allocation issue: just passing y
       affects code generation in the fast path.  */
//        __math_invalidf (y + y);
#endif
    }
}

//#endif

int main() {
    float sinp, cosp;
    libc_sincosf (35.85, &sinp, &cosp);
    printf("sinp: %f, cosp: %f\n", sinp, cosp);
}