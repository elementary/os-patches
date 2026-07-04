/* This file was automatically imported with 
   import_gcry.py. Please don't modify it */
/* hash-common.c - Common code for hash algorithms
 * Copyright (C) 2008 Free Software Foundation, Inc.
 *
 * This file is part of Libgcrypt.
 *
 * Libgcrypt is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * Libgcrypt is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_STDINT_H
#endif

#include "g10lib.h"
#include "bufhelp.h"
#include "hash-common.h"


/* Run a selftest for hash algorithm ALGO.  If the resulting digest
   matches EXPECT/EXPECTLEN and everything else is fine as well,
   return NULL.  If an error occurs, return a static text string
   describing the error.

   DATAMODE controls what will be hashed according to this table:

     0 - Hash the supplied DATA of DATALEN.
     1 - Hash one million times a 'a'.  DATA and DATALEN are ignored.

*/


/* Common function to write a chunk of data to the transform function
   of a hash algorithm.  Note that the use of the term "block" does
   not imply a fixed size block.  Note that we explicitly allow to use
   this function after the context has been finalized; the result does
   not have any meaning but writing after finalize is sometimes
   helpful to mitigate timing attacks. */
void
_gcry_md_block_write (void *context, const void *inbuf_arg, size_t inlen)
{
  const unsigned char *inbuf = inbuf_arg;
  gcry_md_block_ctx_t *hd = context;
  unsigned int stack_burn = 0;
  unsigned int nburn;
  const unsigned int blocksize_shift = hd->blocksize_shift;
  const unsigned int blocksize = 1 << blocksize_shift;
  size_t inblocks;
  size_t copylen;

  if (sizeof(hd->buf) < blocksize)
    BUG();

  if (!hd->bwrite)
    return;

  if (hd->count > blocksize)
    {
      /* This happens only when gcry_md_write is called after final.
       * Writing after final is used for mitigating timing attacks. */
      hd->count = 0;
    }

  while (hd->count)
    {
      if (hd->count == blocksize)  /* Flush the buffer. */
	{
	  nburn = hd->bwrite (hd, hd->buf, 1);
	  stack_burn = nburn > stack_burn ? nburn : stack_burn;
	  hd->count = 0;
	  if (!++hd->nblocks)
	    hd->nblocks_high++;
	}
      else
	{
	  copylen = inlen;
	  if (copylen > blocksize - hd->count)
	    copylen = blocksize - hd->count;

	  if (copylen == 0)
	    break;

	  buf_cpy (&hd->buf[hd->count], inbuf, copylen);
	  hd->count += copylen;
	  inbuf += copylen;
	  inlen -= copylen;
	}
    }

  if (inlen == 0)
    return;

  if (inlen >= blocksize)
    {
      inblocks = inlen >> blocksize_shift;
      nburn = hd->bwrite (hd, inbuf, inblocks);
      stack_burn = nburn > stack_burn ? nburn : stack_burn;
      hd->count = 0;
      hd->nblocks_high += (hd->nblocks + inblocks < inblocks);
      hd->nblocks += inblocks;
      inlen -= inblocks << blocksize_shift;
      inbuf += inblocks << blocksize_shift;
    }

  if (inlen)
    {
      buf_cpy (hd->buf, inbuf, inlen);
      hd->count = inlen;
    }

  if (stack_burn > 0)
    _gcry_burn_stack (stack_burn);
}
