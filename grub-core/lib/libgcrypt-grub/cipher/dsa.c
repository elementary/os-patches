/* This file was automatically imported with 
   import_gcry.py. Please don't modify it */
#include <grub/dl.h>
GRUB_MOD_LICENSE ("GPLv3+");
/* dsa.c - DSA signature algorithm
 * Copyright (C) 1998, 2000, 2001, 2002, 2003,
 *               2006, 2008  Free Software Foundation, Inc.
 * Copyright (C) 2013 g10 Code GmbH.
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


typedef struct
{
  gcry_mpi_t p;	    /* prime */
  gcry_mpi_t q;	    /* group order */
  gcry_mpi_t g;	    /* group generator */
  gcry_mpi_t y;	    /* g^x mod p */
} DSA_public_key;


typedef struct
{
  gcry_mpi_t p;	    /* prime */
  gcry_mpi_t q;	    /* group order */
  gcry_mpi_t g;	    /* group generator */
  gcry_mpi_t y;	    /* g^x mod p */
  gcry_mpi_t x;	    /* secret exponent */
} DSA_secret_key;


/* A structure used to hold domain parameters.  */
typedef struct
{
  gcry_mpi_t p;	    /* prime */
  gcry_mpi_t q;	    /* group order */
  gcry_mpi_t g;	    /* group generator */
} dsa_domain_t;


static const char *dsa_names[] =
  {
    "dsa",
    "openpgp-dsa",
    NULL,
  };


/* A sample 1024 bit DSA key used for the selftests.  Not anymore
 * used, kept only for reference.  */
#if 0
/* A sample 1024 bit DSA key used for the selftests (public only).  */
#endif /*0*/

/* 2048 DSA key from RFC 6979 A.2.2 */




static int check_secret_key (DSA_secret_key *sk);
static gpg_err_code_t verify (gcry_mpi_t r, gcry_mpi_t s, gcry_mpi_t input,
                              DSA_public_key *pkey, int flags, int hashalgo);
static unsigned int dsa_get_nbits (gcry_sexp_t parms);




/* Check the DSA key length is acceptable for key generation or usage */
static gpg_err_code_t
dsa_check_keysize (unsigned int nbits)
{
  if (fips_mode () && nbits < 2048)
    return GPG_ERR_INV_VALUE;

  return 0;
}






/* Check that a freshly generated key actually works.  Returns 0 on success. */



/*
   Generate a DSA key pair with a key of size NBITS.  If transient_key
   is true the key is generated using the standard RNG and not the
   very secure one.

   Returns: 2 structures filled with all needed values
 	    and an array with the n-1 factors of (p-1)
 */


/* Generate a DSA key pair with a key of size NBITS using the
   algorithm given in FIPS-186-3.  If USE_FIPS186_2 is true,
   FIPS-186-2 is used and thus the length is restricted to 1024/160.
   If DERIVEPARMS is not NULL it may contain a seed value.  If domain
   parameters are specified in DOMAIN, DERIVEPARMS may not be given
   and NBITS and QBITS must match the specified domain parameters.  */



/*
   Test whether the secret key is valid.
   Returns: if this is a valid key.
 */
static int
check_secret_key( DSA_secret_key *sk )
{
  int rc;
  gcry_mpi_t y = mpi_alloc( mpi_get_nlimbs(sk->y) );

  mpi_powm( y, sk->g, sk->x, sk->p );
  rc = !mpi_cmp( y, sk->y );
  mpi_free( y );
  return rc;
}



/*
   Make a DSA signature from INPUT and put it into r and s.

   INPUT may either be a plain MPI or an opaque MPI which is then
   internally converted to a plain MPI.  FLAGS and HASHALGO may both
   be 0 for standard operation mode.

   The random value, K_SUPPLIED, may be supplied externally.  If not,
   it is generated internally.

   The return value is 0 on success or an error code.  Note that for
   backward compatibility the function will not return any error if
   FLAGS and HASHALGO are both 0 and INPUT is a plain MPI.
 */


/*
   Returns true if the signature composed from R and S is valid.
 */
static gpg_err_code_t
verify (gcry_mpi_t r, gcry_mpi_t s, gcry_mpi_t input, DSA_public_key *pkey,
        int flags, int hashalgo)
{
  gpg_err_code_t rc = 0;
  gcry_mpi_t w, u1, u2, v;
  gcry_mpi_t base[3];
  gcry_mpi_t ex[3];
  gcry_mpi_t hash;
  unsigned int nbits;
  gcry_mpi_t hash_computed_internally = NULL;

  if( !(mpi_cmp_ui( r, 0 ) > 0 && mpi_cmp( r, pkey->q ) < 0) )
    return GPG_ERR_BAD_SIGNATURE; /* Assertion	0 < r < n  failed.  */
  if( !(mpi_cmp_ui( s, 0 ) > 0 && mpi_cmp( s, pkey->q ) < 0) )
    return GPG_ERR_BAD_SIGNATURE; /* Assertion	0 < s < n  failed.  */

  nbits = mpi_get_nbits (pkey->q);
  if ((flags & PUBKEY_FLAG_PREHASH))
    {
      rc = _gcry_dsa_compute_hash (&hash_computed_internally, input, hashalgo);
      if (rc)
        return rc;
      input = hash_computed_internally;
    }
  rc = _gcry_dsa_normalize_hash (input, &hash, nbits);
  if (rc)
    {
      mpi_free (hash_computed_internally);
      return rc;
    }

  w  = mpi_alloc( mpi_get_nlimbs(pkey->q) );
  u1 = mpi_alloc( mpi_get_nlimbs(pkey->q) );
  u2 = mpi_alloc( mpi_get_nlimbs(pkey->q) );
  v  = mpi_alloc( mpi_get_nlimbs(pkey->p) );

  /* w = s^(-1) mod q */
  mpi_invm( w, s, pkey->q );

  /* u1 = (hash * w) mod q */
  mpi_mulm( u1, hash, w, pkey->q );

  /* u2 = r * w mod q  */
  mpi_mulm( u2, r, w, pkey->q );

  /* v =  g^u1 * y^u2 mod p mod q */
  base[0] = pkey->g; ex[0] = u1;
  base[1] = pkey->y; ex[1] = u2;
  base[2] = NULL;    ex[2] = NULL;
  mpi_mulpowm( v, base, ex, pkey->p );
  mpi_fdiv_r( v, v, pkey->q );

  if (mpi_cmp( v, r ))
    {
      if (DBG_CIPHER)
        {
          log_mpidump ("     i", input);
          log_mpidump ("     h", hash);
          log_mpidump ("     v", v);
          log_mpidump ("     r", r);
          log_mpidump ("     s", s);
        }
      rc = GPG_ERR_BAD_SIGNATURE;
    }

  mpi_free(w);
  mpi_free(u1);
  mpi_free(u2);
  mpi_free(v);
  if (hash != input)
    mpi_free (hash);
  mpi_free (hash_computed_internally);

  return rc;
}


/*********************************************
 **************  interface  ******************
 *********************************************/

#define dsa_generate 0


static gcry_err_code_t
dsa_check_secret_key (gcry_sexp_t keyparms)
{
  gcry_err_code_t rc;
  DSA_secret_key sk = {NULL, NULL, NULL, NULL, NULL};

  rc = _gcry_sexp_extract_param (keyparms, NULL, "pqgyx",
                                  &sk.p, &sk.q, &sk.g, &sk.y, &sk.x,
                                  NULL);
  if (rc)
    goto leave;

  if (!check_secret_key (&sk))
    rc = GPG_ERR_BAD_SECKEY;

 leave:
  _gcry_mpi_release (sk.p);
  _gcry_mpi_release (sk.q);
  _gcry_mpi_release (sk.g);
  _gcry_mpi_release (sk.y);
  _gcry_mpi_release (sk.x);
  if (DBG_CIPHER)
    log_debug ("dsa_testkey    => %s\n", gpg_strerror (rc));
  return rc;
}


#define dsa_sign 0

static gcry_err_code_t
dsa_verify (gcry_sexp_t s_sig, gcry_sexp_t s_data, gcry_sexp_t s_keyparms)
{
  gcry_err_code_t rc;
  struct pk_encoding_ctx ctx;
  gcry_sexp_t l1 = NULL;
  gcry_mpi_t sig_r = NULL;
  gcry_mpi_t sig_s = NULL;
  gcry_mpi_t data = NULL;
  DSA_public_key pk = { NULL, NULL, NULL, NULL };
  unsigned int nbits = dsa_get_nbits (s_keyparms);

  rc = dsa_check_keysize (nbits);
  if (rc)
    return rc;

  _gcry_pk_util_init_encoding_ctx (&ctx, PUBKEY_OP_VERIFY, nbits);

  /* Extract the data.  */
  rc = _gcry_pk_util_data_to_mpi (s_data, &data, &ctx);
  if (rc)
    goto leave;
  if (DBG_CIPHER)
    log_mpidump ("dsa_verify data", data);

  /* Extract the signature value.  */
  rc = _gcry_pk_util_preparse_sigval (s_sig, dsa_names, &l1, NULL);
  if (rc)
    goto leave;
  rc = _gcry_sexp_extract_param (l1, NULL, "rs", &sig_r, &sig_s, NULL);
  if (rc)
    goto leave;
  if (DBG_CIPHER)
    {
      log_mpidump ("dsa_verify  s_r", sig_r);
      log_mpidump ("dsa_verify  s_s", sig_s);
    }

  /* Extract the key.  */
  rc = _gcry_sexp_extract_param (s_keyparms, NULL, "pqgy",
                                 &pk.p, &pk.q, &pk.g, &pk.y, NULL);
  if (rc)
    goto leave;
  if (DBG_CIPHER)
    {
      log_mpidump ("dsa_verify    p", pk.p);
      log_mpidump ("dsa_verify    q", pk.q);
      log_mpidump ("dsa_verify    g", pk.g);
      log_mpidump ("dsa_verify    y", pk.y);
    }

  /* Verify the signature.  */
  rc = verify (sig_r, sig_s, data, &pk, ctx.flags, ctx.hash_algo);

 leave:
  _gcry_mpi_release (pk.p);
  _gcry_mpi_release (pk.q);
  _gcry_mpi_release (pk.g);
  _gcry_mpi_release (pk.y);
  _gcry_mpi_release (data);
  _gcry_mpi_release (sig_r);
  _gcry_mpi_release (sig_s);
  sexp_release (l1);
  _gcry_pk_util_free_encoding_ctx (&ctx);
  if (DBG_CIPHER)
    log_debug ("dsa_verify    => %s\n", rc?gpg_strerror (rc):"Good");
  return rc;
}


/* Return the number of bits for the key described by PARMS.  On error
 * 0 is returned.  The format of PARMS starts with the algorithm name;
 * for example:
 *
 *   (dsa
 *     (p <mpi>)
 *     (q <mpi>)
 *     (g <mpi>)
 *     (y <mpi>))
 *
 * More parameters may be given but we only need P here.
 */
static unsigned int
dsa_get_nbits (gcry_sexp_t parms)
{
  gcry_sexp_t l1;
  gcry_mpi_t p;
  unsigned int nbits;

  l1 = sexp_find_token (parms, "p", 1);
  if (!l1)
    return 0; /* Parameter P not found.  */

  p = sexp_nth_mpi (l1, 1, GCRYMPI_FMT_USG);
  sexp_release (l1);
  nbits = p? mpi_get_nbits (p) : 0;
  _gcry_mpi_release (p);
  return nbits;
}



/*
     Self-test section.
 */





/* Run a full self-test for ALGO and return 0 on success.  */



gcry_pk_spec_t _gcry_pubkey_spec_dsa =
  {
    GCRY_PK_DSA, { 0, 0 },
    GCRY_PK_USAGE_SIGN,
    "DSA", dsa_names,
    "pqgy", "pqgyx", "", "rs", "pqgy",
    dsa_generate,
    dsa_check_secret_key,
    NULL,
    NULL,
    dsa_sign,
    dsa_verify,
    dsa_get_nbits,
    GRUB_UTIL_MODNAME("gcry_dsa")
  };


GRUB_MOD_INIT(gcry_dsa)
{
  grub_crypto_pk_dsa = &_gcry_pubkey_spec_dsa;
}

GRUB_MOD_FINI(gcry_dsa)
{
  grub_crypto_pk_dsa = 0;
}
