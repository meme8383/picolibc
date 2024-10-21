/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright © 2024 Keith Packard
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define _DEFAULT_SOURCE
#include <uchar.h>
#include <wchar.h>
#include <errno.h>
#include <stdint.h>

size_t
c8rtomb (char *s, char8_t c8, mbstate_t *ps)
{
    static NEWLIB_THREAD_LOCAL mbstate_t _state;
    char32_t    c32;
    int count;

    if (ps == NULL)
        ps = &_state;

    if (s == NULL)
        c8 = 0;

    switch (c8 & 0xc0) {
    default:
        if (ps->__count != 0) {
            errno = EILSEQ;
            return (size_t) -1;
        }
        c32 = c8;
        break;
    case 0xc0:
        /*
         * For a two-byte sequence, we need to have at least two bits
         * of contribution from the first byte or the sequence could
         * have been a single byte.
         */
        if (ps->__count != 0 || c8 <= 0xc1) {
            errno = EILSEQ;
            return (size_t) -1;
        }
        count = 1;
        while ((c8 & (1 << (6 - count))) != 0) {
            if (count > 4) {
                errno = EILSEQ;
                return (size_t) -1;
            }
            count++;
        }
        c32 = (c8 & ((1 << (6-count)) - 1)) << (6 * count);

        if (c32 > 0x10ffff)
        {
            errno = EILSEQ;
            return (size_t) -1;
        }

        ps->__value.__ucs = c32;
        ps->__count = -count;
        return 0;
    case 0x80:
        count = -ps->__count;
        c8 &= 0x3f;
        if (count == 0 || (ps->__value.__ucs == 0 && c8 < (0x80 >> count)))
        {
            errno = EILSEQ;
            return (size_t) -1;
        }
        count--;
        c32 = ps->__value.__ucs | (c8 << (6 * count));
        if (c32 > 0x10ffff || (0xd800 <= c32 && c32 <= 0xdfff))
        {
            ps->__count = 0;
            errno = EILSEQ;
            return (size_t) -1;
        }
        ps->__count = -count;
        if (count != 0) {
            ps->__value.__ucs = c32;
            return 0;
        }
        break;
    }

#if __SIZEOF_WCHAR_T__ == 2
    const wchar_t wc[2] = {
        [0] = ((c32 - 0x10000) >> 10) + 0xd800,
        [1] = (c32 & 0x3ff) + 0xdc00
    };
    const wchar_t *wcp = wc;
    return wcsnrtombs(s, &wcp, 2, SIZE_MAX, ps);
#elif __SIZEOF_WCHAR_T__ == 4
    return wcrtomb(s, (wchar_t) c32, ps);
#else
#error wchar_t size unknown
#endif
}
