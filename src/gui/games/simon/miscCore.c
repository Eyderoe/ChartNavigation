// Upstream misc.c pure utility excerpts; see README.md.
/*
 * misc.c: Miscellaneous helpful functions.
 */

#include <assert.h>
#include <ctype.h>
#ifdef NO_TGMATH_H
#  include <math.h>
#else
#  include <tgmath.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "puzzles.h"

char MOVE_UI_UPDATE[] = "";
char MOVE_NO_EFFECT[] = "";
char MOVE_UNUSED[] = "";

void free_cfg(config_item *cfg)
{
    config_item *i;

    for (i = cfg; i->type != C_END; i++)
	if (i->type == C_STRING)
	    sfree(i->u.string.sval);
    sfree(cfg);
}

void free_keys(key_label *keys, int nkeys)
{
    int i;

    for(i = 0; i < nkeys; i++)
        sfree(keys[i].label);
    sfree(keys);
}

/*
 * The Mines (among others) game descriptions contain the location of every
 * mine, and can therefore be used to cheat.
 *
 * It would be pointless to attempt to _prevent_ this form of
 * cheating by encrypting the description, since Mines is
 * open-source so anyone can find out the encryption key. However,
 * I think it is worth doing a bit of gentle obfuscation to prevent
 * _accidental_ spoilers: if you happened to note that the game ID
 * starts with an F, for example, you might be unable to put the
 * knowledge of those mines out of your mind while playing. So,
 * just as discussions of film endings are rot13ed to avoid
 * spoiling it for people who don't want to be told, we apply a
 * keyless, reversible, but visually completely obfuscatory masking
 * function to the mine bitmap.
 */
void obfuscate_bitmap(unsigned char *bmp, int bits, bool decode)
{
    int bytes, firsthalf, secondhalf;
    struct step {
	unsigned char *seedstart;
	int seedlen;
	unsigned char *targetstart;
	int targetlen;
    } steps[2];
    int i, j;

    /*
     * My obfuscation algorithm is similar in concept to the OAEP
     * encoding used in some forms of RSA. Here's a specification
     * of it:
     * 
     * 	+ We have a `masking function' which constructs a stream of
     * 	  pseudorandom bytes from a seed of some number of input
     * 	  bytes.
     * 
     * 	+ We pad out our input bit stream to a whole number of
     * 	  bytes by adding up to 7 zero bits on the end. (In fact
     * 	  the bitmap passed as input to this function will already
     * 	  have had this done in practice.)
     * 
     * 	+ We divide the _byte_ stream exactly in half, rounding the
     * 	  half-way position _down_. So an 81-bit input string, for
     * 	  example, rounds up to 88 bits or 11 bytes, and then
     * 	  dividing by two gives 5 bytes in the first half and 6 in
     * 	  the second half.
     * 
     * 	+ We generate a mask from the second half of the bytes, and
     * 	  XOR it over the first half.
     * 
     * 	+ We generate a mask from the (encoded) first half of the
     * 	  bytes, and XOR it over the second half. Any null bits at
     * 	  the end which were added as padding are cleared back to
     * 	  zero even if this operation would have made them nonzero.
     * 
     * To de-obfuscate, the steps are precisely the same except
     * that the final two are reversed.
     * 
     * Finally, our masking function. Given an input seed string of
     * bytes, the output mask consists of concatenating the SHA-1
     * hashes of the seed string and successive decimal integers,
     * starting from 0.
     */

    bytes = (bits + 7) / 8;
    firsthalf = bytes / 2;
    secondhalf = bytes - firsthalf;

    steps[decode ? 1 : 0].seedstart = bmp + firsthalf;
    steps[decode ? 1 : 0].seedlen = secondhalf;
    steps[decode ? 1 : 0].targetstart = bmp;
    steps[decode ? 1 : 0].targetlen = firsthalf;

    steps[decode ? 0 : 1].seedstart = bmp;
    steps[decode ? 0 : 1].seedlen = firsthalf;
    steps[decode ? 0 : 1].targetstart = bmp + firsthalf;
    steps[decode ? 0 : 1].targetlen = secondhalf;

    for (i = 0; i < 2; i++) {
	SHA_State base, final;
	unsigned char digest[20];
	char numberbuf[80];
	int digestpos = 20, counter = 0;

	SHA_Init(&base);
	SHA_Bytes(&base, steps[i].seedstart, steps[i].seedlen);

	for (j = 0; j < steps[i].targetlen; j++) {
	    if (digestpos >= 20) {
		sprintf(numberbuf, "%d", counter++);
		final = base;
		SHA_Bytes(&final, numberbuf, strlen(numberbuf));
		SHA_Final(&final, digest);
		digestpos = 0;
	    }
	    steps[i].targetstart[j] ^= digest[digestpos++];
	}

	/*
	 * Mask off the pad bits in the final byte after both steps.
	 */
	if (bits % 8)
	    bmp[bits / 8] &= 0xFF & (0xFF00 >> (bits % 8));
    }
}

/* err, yeah, these two pretty much rely on unsigned char being 8 bits.
 * Platforms where this is not the case probably have bigger problems
 * than just making these two work, though... */
char *bin2hex(const unsigned char *in, int inlen)
{
    char *ret = snewn(inlen*2 + 1, char), *p = ret;
    int i;

    for (i = 0; i < inlen*2; i++) {
        int v = in[i/2];
        if (i % 2 == 0) v >>= 4;
        *p++ = "0123456789abcdef"[v & 0xF];
    }
    *p = '\0';
    return ret;
}

unsigned char *hex2bin(const char *in, int outlen)
{
    unsigned char *ret = snewn(outlen, unsigned char);
    int i;

    memset(ret, 0, outlen*sizeof(unsigned char));
    for (i = 0; i < outlen*2; i++) {
        int c = in[i];
        int v;

        assert(c != 0);
        if (c >= '0' && c <= '9')
            v = c - '0';
        else if (c >= 'a' && c <= 'f')
            v = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F')
            v = c - 'A' + 10;
        else
            v = 0;

        ret[i / 2] |= v << (4 * (1 - (i % 2)));
    }
    return ret;
}

char *fgetline(FILE *fp)
{
    char *ret = snewn(512, char);
    int size = 512, len = 0;
    while (fgets(ret + len, size - len, fp)) {
	len += strlen(ret + len);
	if (ret[len-1] == '\n')
	    break;		       /* got a newline, we're done */
	size = len + 512;
	ret = sresize(ret, size, char);
    }
    if (len == 0) {		       /* first fgets returned NULL */
	sfree(ret);
	return NULL;
    }
    ret[len] = '\0';
    return ret;
}

int getenv_bool(const char *name, int dflt)
{
    char *env = getenv(name);
    if (env == NULL) return dflt;
    if (strchr("yYtT", env[0])) return true;
    return false;
}

/* Utility functions for colour manipulation. */

void swap_regions(void *av, void *bv, size_t size)
{
    char tmpbuf[512];
    char *a = av, *b = bv;

    while (size > 0) {
	int thislen = min(size, sizeof(tmpbuf));
	memcpy(tmpbuf, a, thislen);
	memcpy(a, b, thislen);
	memcpy(b, tmpbuf, thislen);
	a += thislen;
	b += thislen;
	size -= thislen;
    }
}

void shuffle(void *array, int nelts, int eltsize, random_state *rs)
{
    char *carray = (char *)array;
    int i;

    for (i = nelts; i-- > 1 ;) {
        int j = random_upto(rs, i+1);
        if (j != i)
            swap_regions(carray + eltsize * i, carray + eltsize * j, eltsize);
    }
}

int compare_integers(const void *av, const void *bv) {
    const int *a = (const int *)av;
    const int *b = (const int *)bv;
    if (*a < *b)
	return -1;
    else if (*a > *b)
	return +1;
    else
	return 0;
}

int n_times_root_k(int n_signed, int k)
{
    unsigned x, r, m;
    int sign = n_signed < 0 ? -1 : +1;
    unsigned n = n_signed * sign;
    unsigned bitpos;

    /*
     * Method:
     *
     * We transform m gradually from zero into n, by multiplying it by
     * 2 in each step and optionally adding 1, so that it's always
     * floor(n/2^something).
     *
     * At the start of each step, x is the largest integer less than
     * or equal to m*sqrt(k). We transform m to 2m+bit, and therefore
     * we must transform x to 2x+something to match. The 'something'
     * we add to 2x is at most floor(sqrt(k))+2. (Worst case is if m
     * sqrt(k) was equal to x + 1-eps for some tiny eps, and then the
     * incoming bit of m is 1, so that (2m+1)sqrt(k) =
     * 2x+2+sqrt(k)-2eps.)
     *
     * To compute this, we also track the residual value r such that
     * x^2+r = km^2.
     *
     * The algorithm below is very similar to the usual approach for
     * taking the square root of an integer in binary. The wrinkle is
     * that we have an integer multiplier, i.e. we're computing
     * n*sqrt(k) rather than just sqrt(k). Of course in principle we
     * could just take sqrt(n^2k), but we'd need an integer twice the
     * width to hold n^2. Pulling out n and treating it specially
     * makes overflow less likely.
     */

    x = r = m = 0;

    for (bitpos = UINT_MAX & ~(UINT_MAX >> 1); bitpos; bitpos >>= 1) {
        unsigned a, b = (n & bitpos) ? 1 : 0;

        /*
         * Check invariants. We expect that x^2 + r = km^2 (i.e. our
         * residual term is correct), and also that r < 2x+1 (because
         * if not, then we could replace x with x+1 and still get a
         * value that made r non-negative, i.e. x would not be the
         * _largest_ integer less than m sqrt(k)).
         */
        assert(x*x + r == k*m*m);
        assert(r < 2*x+1);

        /*
         * We're going to replace m with 2m+b, and x with 2x+a for
         * some a we haven't decided on yet.
         *
         * The new value of the residual will therefore be
         *
         *   k (2m+b)^2 - (2x+a)^2
         * = (4km^2 + 4kmb + kb^2) - (4x^2 + 4xa + a^2)
         * = 4 (km^2 - x^2) + 4kmb + kb^2 - 4xa - a^2
         * = 4r + 4kmb + kb^2 - 4xa - a^2          (because r = km^2 - x^2)
         * = 4r + (4m + 1)kb - 4xa - a^2           (b is 0 or 1, so b = b^2)
         */
        for (a = 0;; a++) {
            /* If we made this routine handle square roots of numbers
             * significantly bigger than 3 or 5 then it would be
             * sensible to make this a binary search. Here, it hardly
             * seems important. */
            unsigned pos = 4*r + k*b*(4*m + 1);
            unsigned neg = 4*a*x + a*a;
            if (pos < neg)
                break;                 /* this value of a is too big */
        }

        /* The above loop will have terminated with a one too big. So
         * now decrementing a will give us the right value to add. */
        a--;

        r = 4*r + b*k*(4*m + 1) - (4*a*x + a*a);
        m = 2*m+b;
        x = 2*x+a;
    }

    /*
     * Finally, round to the nearest integer. At present, x is the
     * largest integer that is _at most_ m sqrt(k). But we want the
     * _nearest_ integer, whether that's rounded up or down. So check
     * whether (x + 1/2) is still less than m sqrt(k), i.e. whether
     * (x + 1/2)^2 < km^2; if it is, then we increment x.
     *
     * We have km^2 - (x + 1/2)^2 = km^2 - x^2 - x - 1/4
     *                            = r - x - 1/4
     *
     * and since r and x are integers, this is greater than 0 if and
     * only if r > x.
     *
     * (There's no need to worry about tie-breaking exact halfway
     * rounding cases. sqrt(k) is irrational, so none such exist.)
     */
    if (r > x)
        x++;

    /*
     * Put the sign back on, and convert back from unsigned to int.
     */
    if (sign == +1) {
        return x;
    } else {
        /* Be a little careful to avoid compilers deciding I've just
         * perpetrated signed-integer overflow. This should optimise
         * down to no actual code. */
        return INT_MIN + (int)(-x - (unsigned)INT_MIN);
    }
}

/* vim: set shiftwidth=4 tabstop=8: */
