/* This file was automatically imported with 
   import_gcry.py. Please don't modify it */
/* dsa-common.c - Common code for DSA
 * Copyright (C) 1998, 1999 Free Software Foundation, Inc.
 * Copyright (C) 2013  g10 Code GmbH
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


#include "g10lib.h"
#include "mpi.h"
#include "cipher.h"
#include "pubkey-internal.h"


/*
 * Modify K, so that computation time difference can be small,
 * by making K large enough.
 *
 * Originally, (EC)DSA computation requires k where 0 < k < q.  Here,
 * we add q (the order), to keep k in a range: q < k < 2*q (or,
 * addming more q, to keep k in a range: 2*q < k < 3*q), so that
 * timing difference of the EC multiply (or exponentiation) operation
 * can be small.  The result of (EC)DSA computation is same.
 */
void
_gcry_dsa_modify_k (gcry_mpi_t k, gcry_mpi_t q, int qbits)
{
  gcry_mpi_t k1 = mpi_new (qbits+2);

  mpi_resize (k, (qbits+2+BITS_PER_MPI_LIMB-1) / BITS_PER_MPI_LIMB);
  k->nlimbs = k->alloced;
  mpi_add (k, k, q);
  mpi_add (k1, k, q);
  mpi_set_cond (k, k1, !mpi_test_bit (k, qbits));

  mpi_free (k1);
}

/*
 * Generate a random secret exponent K less than Q.
 * Note that ECDSA uses this code also to generate D.
 */
gcry_mpi_t
_gcry_dsa_gen_k (gcry_mpi_t q, int security_level)
{
  gcry_mpi_t k        = mpi_alloc_secure (mpi_get_nlimbs (q));
  unsigned int nbits  = mpi_get_nbits (q);
  unsigned int nbytes = (nbits+7)/8;
  char *rndbuf = NULL;

  /* To learn why we don't use mpi_mod to get the requested bit size,
     read the paper: "The Insecurity of the Digital Signature
     Algorithm with Partially Known Nonces" by Nguyen and Shparlinski.
     Journal of Cryptology, New York. Vol 15, nr 3 (2003)  */

  if (DBG_CIPHER)
    log_debug ("choosing a random k of %u bits at seclevel %d\n",
               nbits, security_level);
  for (;;)
    {
      if ( !rndbuf || nbits < 32 )
        {
          xfree (rndbuf);
          rndbuf = _gcry_random_bytes_secure (nbytes, security_level);
	}
      else
        { /* Change only some of the higher bits.  We could improve
	     this by directly requesting more memory at the first call
	     to get_random_bytes() and use these extra bytes here.
	     However the required management code is more complex and
	     thus we better use this simple method.  */
          char *pp = _gcry_random_bytes_secure (4, security_level);
          memcpy (rndbuf, pp, 4);
          xfree (pp);
	}
      _gcry_mpi_set_buffer (k, rndbuf, nbytes, 0);

      /* Make sure we have the requested number of bits.  This code
         looks a bit funny but it is easy to understand if you
         consider that mpi_set_highbit clears all higher bits.  We
         don't have a clear_highbit, thus we first set the high bit
         and then clear it again.  */
      if (mpi_test_bit (k, nbits-1))
        mpi_set_highbit (k, nbits-1);
      else
        {
          mpi_set_highbit (k, nbits-1);
          mpi_clear_bit (k, nbits-1);
	}

      if (!(mpi_cmp (k, q) < 0))    /* check: k < q */
        {
          if (DBG_CIPHER)
            log_debug ("\tk too large - again\n");
          continue; /* no  */
        }
      if (!(mpi_cmp_ui (k, 0) > 0)) /* check: k > 0 */
        {
          if (DBG_CIPHER)
            log_debug ("\tk is zero - again\n");
          continue; /* no */
        }
      break;	/* okay */
    }
  xfree (rndbuf);

  return k;
}


/* Turn VALUE into an octet string and store it in an allocated buffer
   at R_FRAME.  If the resulting octet string is shorter than NBYTES
   the result will be left padded with zeroes.  If VALUE does not fit
   into NBYTES an error code is returned.  */


/* Connert the bit string BITS of length NBITS into an octet string
   with a length of (QBITS+7)/8 bytes.  On success store the result at
   R_FRAME.  */


/*
 * Generate a deterministic secret exponent K less than DSA_Q.  H1 is
 * the to be signed digest with a length of HLEN bytes.  HALGO is the
 * algorithm used to create the hash.  On success the value for K is
 * stored at R_K.
 */



/*
 * For DSA/ECDSA, as prehash function, compute hash with HASHALGO for
 * INPUT.  Result hash value is returned in R_HASH as an opaque MPI.
 * Returns error code.
 */
gpg_err_code_t
_gcry_dsa_compute_hash (gcry_mpi_t *r_hash, gcry_mpi_t input, int hashalgo)
{
  gpg_err_code_t rc = 0;
  size_t hlen;
  void *hashbuf;
  void *abuf;
  unsigned int abits;
  unsigned int n;

  hlen = _gcry_md_get_algo_dlen (hashalgo);
  hashbuf = xtrymalloc (hlen);
  if (!hashbuf)
    {
      rc = gpg_err_code_from_syserror ();
      return rc;
    }

  if (mpi_is_opaque (input))
    {
      abuf = mpi_get_opaque (input, &abits);
      n = (abits+7)/8;
      _gcry_md_hash_buffer (hashalgo, hashbuf, abuf, n);
    }
  else
    {
      abits = mpi_get_nbits (input);
      n = (abits+7)/8;
      abuf = xtrymalloc (n);
      if (!abuf)
        {
          rc = gpg_err_code_from_syserror ();
          xfree (hashbuf);
          return rc;
        }
      _gcry_mpi_to_octet_string (NULL, abuf, input, n);
      _gcry_md_hash_buffer (hashalgo, hashbuf, abuf, n);
      xfree (abuf);
    }

  *r_hash = mpi_set_opaque (NULL, hashbuf, hlen*8);
  if (!*r_hash)
    rc = GPG_ERR_INV_OBJ;

  return rc;
}


/*
 * Truncate opaque hash value to qbits for DSA.
 * Non-opaque input is not truncated, in hope that user
 * knows what is passed. It is not possible to correctly
 * trucate non-opaque inputs.
 */
gpg_err_code_t
_gcry_dsa_normalize_hash (gcry_mpi_t input,
                          gcry_mpi_t *out,
                          unsigned int qbits)
{
  gpg_err_code_t rc = 0;
  const void *abuf;
  unsigned int abits;
  gcry_mpi_t hash;

  if (mpi_is_opaque (input))
    {
      abuf = mpi_get_opaque (input, &abits);
      rc = _gcry_mpi_scan (&hash, GCRYMPI_FMT_USG, abuf, (abits+7)/8, NULL);
      if (rc)
        return rc;
      if (abits > qbits)
        mpi_rshift (hash, hash, abits - qbits);
    }
  else
    hash = input;

  *out = hash;

  return rc;
}
