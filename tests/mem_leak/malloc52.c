/*
 * Never free
 *
 *  Created on: Jan 3, 2012
 *      Author: Yulei Sui
 */

#include "aliascheck.h"

typedef unsigned char  uch;
typedef unsigned short ush;
typedef unsigned long  ulg;

#define BMAX 16         /* maximum bit length of any code (16 for explode) */
#define N_MAX 288       /* maximum number of codes in any set */
#define NULL 0

unsigned hufts;         /* track memory usage */

struct huft {
  uch e;                /* number of extra bits or operation */
  uch b;                /* number of bits in this code or subcode */
  union {
    ush n;              /* literal, length base, or distance base */
    struct huft *t;     /* pointer to next level of table */
  } v;
};

int huft_free(t)
	struct huft *t;         /* table to free */
	/* Free the malloc'ed tables built by huft_build(), which makes a linked
	      list of the tables it made, with the links in a dummy first entry of
		     each table. */
{
	  register struct huft *p, *q;


	    /* Go through linked list, freeing from the malloced (t[-1]) address. */
	    p = t;
		  while (p != (struct huft *)NULL)
			    {
					    q = (--p)->v.t;
						    free((char*)p);
							    p = q;
								  }
		    return 0;
}

int huft_build(b, n, s, d, e, t, m)
unsigned *b;            /* code lengths in bits (all assumed <= BMAX) */
unsigned n;             /* number of codes (assumed <= N_MAX) */
unsigned s;             /* number of simple-valued codes (0..s-1) */
ush *d;                 /* list of base values for non-simple codes */
ush *e;                 /* list of extra bits for non-simple codes */
struct huft **t;        /* result: starting table */
int *m;                 /* maximum lookup bits, returns actual */

{
  unsigned a;                   /* counter for codes of length k */
  unsigned c[BMAX+1];           /* bit length count table */
  unsigned f;                   /* i repeats in table every f entries */
  int g;                        /* maximum code length */
  int h;                        /* table level */
  register unsigned i;          /* counter, current code */
  register unsigned j;          /* counter */
  register int k;               /* number of bits in current code */
  int l;                        /* bits per table (returned in m) */
  register unsigned *p;         /* pointer into c[], b[], or v[] */
  register struct huft *q;      /* points to current table */
  struct huft r;                /* table entry for structure assignment */
  struct huft *u[BMAX];         /* table stack */
  unsigned v[N_MAX];            /* values in order of bit length */
  register int w;               /* bits before this table == (l * h) */
  unsigned x[BMAX+1];           /* bit offsets, then code stack */
  unsigned *xp;                 /* pointer into x */
  int y;                        /* number of dummy codes added */
  unsigned z;                   /* number of entries in current table */


  /* Generate counts for each bit length */

//  p = b;  i = n;
//  do {
//
//    c[*p]++;                    /* assume all entries <= BMAX */
//    p++;                      /* Can't combine with above line (Solaris bug) */
//  } while (--i);
//  if (c[0] == n)                /* null input--all zero length codes */
//  {
//    *t = (struct huft *)NULL;
//    *m = 0;
//    return 0;
//  }
//
//
//  /* Find minimum and maximum length, bound *m by those */
//  l = *m;
//  for (j = 1; j <= BMAX; j++)
//    if (c[j])
//      break;
//  k = j;                        /* minimum code length */
//  if ((unsigned)l < j)
//    l = j;
//  for (i = BMAX; i; i--)
//    if (c[i])
//      break;
//  g = i;                        /* maximum code length */
//  if ((unsigned)l > i)
//    l = i;
//  *m = l;
//
//
//  /* Adjust last length count to fill out codes, if needed */
//  for (y = 1 << j; j < i; j++, y <<= 1)
//    if ((y -= c[j]) < 0)
//      return 2;                 /* bad input: more codes than bits */
//  if ((y -= c[i]) < 0)
//    return 2;
//  c[i] += y;
//
//
//  /* Generate starting offsets into the value table for each length */
//  x[1] = j = 0;
//  p = c + 1;  xp = x + 2;
//  while (--i) {                 /* note that i == g from above */
//    *xp++ = (j += *p++);
//  }
//
//
//  /* Make a table of values in order of bit lengths */
//  p = b;  i = 0;
//  do {
//    if ((j = *p++) != 0)
//      v[x[j]++] = i;
//  } while (++i < n);


//  /* Generate the Huffman codes and for each, make the table entries */
//  x[0] = i = 0;                 /* first Huffman code is zero */
//  p = v;                        /* grab values in bit order */
//  h = -1;                       /* no tables yet--level -1 */
//  w = -l;                       /* bits decoded == (l * h) */
//  u[0] = (struct huft *)NULL;   /* just to keep compilers happy */
//  q = (struct huft *)NULL;      /* ditto */
//  z = 0;                        /* ditto */

  /* go through the bit lengths (k already is bits in shortest code) */
//  for (; k <= g; k++)
//  {
//    a = c[k];
    while (a--)
    {
      /* here i is the Huffman code of length k bits for value *p */
      /* make tables up to required level */
      while (k > w + l)
      {
        h++;
        w += l;                 /* previous table always l bits */

        /* compute minimum size table less than or equal to l bits */
        z = (z = g - w) > (unsigned)l ? l : z;  /* upper limit on table size */
        if ((f = 1 << (j = k - w)) > a + 1)     /* try a k-w bit table */
        {                       /* too few codes for k-w bit table */
          f -= a + 1;           /* deduct codes from patterns left */
          xp = c + k;
          while (++j < z)       /* try smaller tables up to z bits */
          {
            if ((f <<= 1) <= *++xp)
              break;            /* enough codes to use up j bits */
            f -= *xp;           /* else deduct codes from patterns */
          }
        }
        z = 1 << j;             /* table entries for j-bit table */

        /* allocate and link in new table */
        if ((q = (struct huft *)PLKMALLOC((z + 1)*sizeof(struct huft))) ==
            (struct huft *)NULL)
        {
          if (h)
            huft_free(u[0]);
          return 3;             /* not enough memory */
        }
        hufts += z + 1;         /* track memory usage */
        *t = q + 1;             /* link to list for huft_free() */
        *(t = &(q->v.t)) = (struct huft *)NULL;
        u[h] = ++q;             /* table starts after link */

        /* connect to last table, if there is one */
        if (h)
        {
          x[h] = i;             /* save pattern for backing up */
          r.b = (uch)l;         /* bits to dump before this table */
          r.e = (uch)(16 + j);  /* bits in this table */
          r.v.t = q;            /* pointer to this table */
          j = i >> (w - l);     /* (get around Turbo C bug) */
          u[h-1][j] = r;        /* connect to last table */
        }
      }

      /* set up table entry in r */
//      r.b = (uch)(k - w);
//      if (p >= v + n)
//        r.e = 99;               /* out of values--invalid code */
//      else if (*p < s)
//      {
//        r.e = (uch)(*p < 256 ? 16 : 15);    /* 256 is end-of-block code */
//        r.v.n = (ush)(*p);             /* simple code is just the value */
//	p++;                           /* one compiler does not like *p++ */
//      }
//      else
//      {
//        r.e = (uch)e[*p - s];   /* non-simple--look up in lists */
//        r.v.n = d[*p++ - s];
//      }

      /* fill code-like entries with r */
      f = 1 << (k - w);
      for (j = i >> w; j < z; j += f)
        q[j] = r;

      /* backwards increment the k-bit code i */
      for (j = 1 << (k - 1); i & j; j >>= 1)
        i ^= j;
      i ^= j;

      /* backup over finished tables */
      while ((i & ((1 << w) - 1)) != x[h])
      {
        h--;                    /* don't need to update q */
        w -= l;
      }
    }
//  }


  /* Return true (1) if we were given an incomplete table */
  return y != 0 && g != 1;
}

int main(){
	  unsigned ll[286+30];
	  struct huft *tl;      /* literal/length code table */
	  int bl;

	huft_build(ll, 19, 19, NULL, NULL, &tl, &bl);

}
