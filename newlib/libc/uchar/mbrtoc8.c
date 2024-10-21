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

#include <uchar.h>
#include <wchar.h>
#include <errno.h>

size_t
mbrtoc8(char8_t * __restrict pc8, const char * __restrict s, size_t n,
        mbstate_t * __restrict ps)
{
    static NEWLIB_THREAD_LOCAL mbstate_t local_state;

    if (ps == NULL)
        ps = &local_state;

    if (ps->__count < 0) {
        int count = -ps->__count;

        count--;
        *pc8 = 0x80 | ((ps->__value.__ucs >> (6*count)) & 0x3f);
        ps->__count = -count;
        return (size_t) -3;
    }

    char32_t    c32;
    wchar_t     wc;
    size_t      ret;

#if __SIZEOF_WCHAR_T__ == 2

    /*
     * All of the complexity here deals with UTF-8 locales with 2-byte
     * wchar. In this case, for code points outside the BMP, we have
     * to wait until mbrtowc returns a low surrogate and then go
     * extract the rest of the character from the mbstate bits. This
     * uses a lot of knowledge about the internals of how
     * __utf8_mbtowc works.
     */

    /*
     * If mbrtowc returned a high surrogate last time and we didn't
     * receive the low surrogate right away, we need to skip over the
     * first three bytes of the UTF-8 sequence as they would have been
     * consumed generating the high surrogate, but we couldn't tell
     * the caller about that as we had to return -2 waiting for the
     * rest of the character to arrive.
     */
    if (ps->__count == 4) {
        if (n < 3)
            return (size_t) -2;
        s += 3;
        n -= 3;
    }
#endif

    ret = mbrtowc(&wc, s, n, ps);

    switch (ret) {
    case (size_t) -2:       /* wc not stored */
        return ret;
    case (size_t) -1:       /* error */
        return ret;
    default:
        break;
    }

#if __SIZEOF_WCHAR_T__ == 2

    s += ret;
    n -= ret;

    /* Check for high surrogate */
    if (0xd800 <= wc && wc <= 0xdbff) {
        size_t r;
        /* See if the low surrogate is ready yet */
        r = mbrtowc(&wc, s, n, ps);
        switch (r) {
        case (size_t) -2:       /* wc not stored */
            return r;
        case (size_t) -1:       /* error */
            return r;
        default:
            break;
        }
        ret += r;
    }

    /* Check for low surrogate */
    if (0xdc00 <= wc && wc <= 0xdfff) {
        /*
         * The first three bytes are left in the buffer, go fetch them
         * and add in the low six bits in the low surrogate
         */
        c32 = ((((char32_t)ps->__value.__wchb[0] & 0x07) << 18) |
               (((char32_t)ps->__value.__wchb[1] & 0x3f) << 12) |
               (((char32_t)ps->__value.__wchb[2] & 0x3f) << 6) |
               ((char32_t)wc & 0x3f));
    } else {
        c32 = (char32_t) wc;
    }

#elif __SIZEOF_WCHAR_T__ == 4

    c32 = (char32_t) wc;

#else
#error wchar_t size unknown
#endif

    if (c32 > 0x10ffff || (0xd800 <= c32 && c32 <= 0xdfff)) {
        errno = EILSEQ;
        return (size_t) -1;
    }

    if (c32 >= 0x80) {
        int count = 1;
        char8_t c8;

        c8 = 0xc0;
        while (c32 >= ((char32_t) 1 << (6*count + (6 - count)))) {
            c8 |= (1 << (6 - count));
            count++;
        }
        c8 |= (c32 >> (6 * count));
        ps->__count = -count;
        ps->__value.__ucs = c32;
        *pc8 = c8;
    } else {
        *pc8 = (char8_t) c32;
    }
    return ret;
}
