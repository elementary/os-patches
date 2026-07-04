/* This file was automatically imported with 
   import_gcry.py. Please don't modify it */
#include <grub/dl.h>
GRUB_MOD_LICENSE ("GPLv3+");
/* rsa.c - RSA implementation
 * Copyright (C) 1997, 1998, 1999 by Werner Koch (dd9jn)
 * Copyright (C) 2000, 2001, 2002, 2003, 2008 Free Software Foundation, Inc.
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

/* This code uses an algorithm protected by U.S. Patent #4,405,829
   which expired on September 20, 2000.  The patent holder placed that
   patent into the public domain on Sep 6th, 2000.
*/


#include "g10lib.h"
#include "mpi.h"
#include "cipher.h"
#include "pubkey-internal.h"
#include "const-time.h"


typedef struct
{
  gcry_mpi_t n;	    /* modulus */
  gcry_mpi_t e;	    /* exponent */
} RSA_public_key;


typedef struct
{
  gcry_mpi_t n;	    /* public modulus */
  gcry_mpi_t e;	    /* public exponent */
  gcry_mpi_t d;	    /* exponent */
  gcry_mpi_t p;	    /* prime  p. */
  gcry_mpi_t q;	    /* prime  q. */
  gcry_mpi_t u;	    /* inverse of p mod q. */
} RSA_secret_key;


static const char *rsa_names[] =
  {
    "rsa",
    "openpgp-rsa",
    "oid.1.2.840.113549.1.1.1",
    NULL,
  };


/* A sample 2048 bit RSA key used for the selftests.  */

/* A sample 2048 bit RSA key used for the selftests (public only).  */


static int  check_secret_key (RSA_secret_key *sk);
static void public (gcry_mpi_t output, gcry_mpi_t input, RSA_public_key *skey);
static unsigned int rsa_get_nbits (gcry_sexp_t parms);


/* Check that a freshly generated key actually works.  Returns 0 on success. */


/* Callback used by the prime generation to test whether the exponent
   is suitable. Returns 0 if the test has been passed. */

/****************
 * Generate a key pair with a key of size NBITS.
 * USE_E = 0 let Libcgrypt decide what exponent to use.
 *       = 1 request the use of a "secure" exponent; this is required by some
 *           specification to be 65537.
 *       > 2 Use this public exponent.  If the given exponent
 *           is not odd one is internally added to it.
 * TRANSIENT_KEY:  If true, generate the primes using the standard RNG.
 * Returns: 2 structures filled with all needed values
 */


/* Check the RSA key length is acceptable for key generation or usage */
static gpg_err_code_t
rsa_check_keysize (unsigned int nbits)
{
  if (fips_mode () && nbits < 2048)
    return GPG_ERR_INV_VALUE;

  return GPG_ERR_NO_ERROR;
}


/* Check the RSA key length is acceptable for signature verification
 *
 * FIPS allows signature verification with RSA keys of size
 * 1024, 1280, 1536 and 1792 in legacy mode, but this is up to the
 * calling application to decide if the signature is legacy and
 * should be accepted.
 */
static gpg_err_code_t
rsa_check_verify_keysize (unsigned int nbits)
{
  if (fips_mode ())
    {
      if ((nbits >= 1024 && (nbits % 256) == 0) || nbits >= 2048)
        return GPG_ERR_NO_ERROR;

      return GPG_ERR_INV_VALUE;
    }

  return GPG_ERR_NO_ERROR;
}


/****************
 * Generate a key pair with a key of size NBITS.
 * USE_E = 0 let Libcgrypt decide what exponent to use.
 *       = 1 request the use of a "secure" exponent; this is required by some
 *           specification to be 65537.
 *       > 2 Use this public exponent.  If the given exponent
 *           is not odd one is internally added to it.
 * TESTPARMS: If set, do not generate but test whether the p,q is probably prime
 *            Returns key with zeroes to not break code calling this function.
 * TRANSIENT_KEY:  If true, generate the primes using the standard RNG.
 * Returns: 2 structures filled with all needed values
 */


/* Helper for generate_x931.  */


/* Helper for generate_x931.  */



/* Variant of the standard key generation code using the algorithm
   from X9.31.  Using this algorithm has the advantage that the
   generation can be made deterministic which is required for CAVS
   testing.  */


/****************
 * Test whether the secret key is valid.
 * Returns: true if this is a valid key.
 */
static int
check_secret_key( RSA_secret_key *sk )
{
  int rc;
  gcry_mpi_t temp = mpi_alloc( mpi_get_nlimbs(sk->p)*2 );

  mpi_mul(temp, sk->p, sk->q );
  rc = mpi_cmp( temp, sk->n );
  mpi_free(temp);
  return !rc;
}



/****************
 * Public key operation. Encrypt INPUT with PKEY and put result into OUTPUT.
 *
 *	c = m^e mod n
 *
 * Where c is OUTPUT, m is INPUT and e,n are elements of PKEY.
 */
static void
public(gcry_mpi_t output, gcry_mpi_t input, RSA_public_key *pkey )
{
  if( output == input )  /* powm doesn't like output and input the same */
    {
      gcry_mpi_t x = mpi_alloc( mpi_get_nlimbs(input)*2 );
      mpi_powm( x, input, pkey->e, pkey->n );
      mpi_set(output, x);
      mpi_free(x);
    }
  else
    mpi_powm( output, input, pkey->e, pkey->n );
}

#if 0
static void
stronger_key_check ( RSA_secret_key *skey )
{
  gcry_mpi_t t = mpi_alloc_secure ( 0 );
  gcry_mpi_t t1 = mpi_alloc_secure ( 0 );
  gcry_mpi_t t2 = mpi_alloc_secure ( 0 );
  gcry_mpi_t phi = mpi_alloc_secure ( 0 );

  /* check that n == p * q */
  mpi_mul( t, skey->p, skey->q);
  if (mpi_cmp( t, skey->n) )
    log_info ( "RSA Oops: n != p * q\n" );

  /* check that p is less than q */
  if( mpi_cmp( skey->p, skey->q ) > 0 )
    {
      log_info ("RSA Oops: p >= q - fixed\n");
      _gcry_mpi_swap ( skey->p, skey->q);
    }

    /* check that e divides neither p-1 nor q-1 */
    mpi_sub_ui(t, skey->p, 1 );
    mpi_fdiv_r(t, t, skey->e );
    if ( !mpi_cmp_ui( t, 0) )
        log_info ( "RSA Oops: e divides p-1\n" );
    mpi_sub_ui(t, skey->q, 1 );
    mpi_fdiv_r(t, t, skey->e );
    if ( !mpi_cmp_ui( t, 0) )
        log_info ( "RSA Oops: e divides q-1\n" );

    /* check that d is correct */
    mpi_sub_ui( t1, skey->p, 1 );
    mpi_sub_ui( t2, skey->q, 1 );
    mpi_mul( phi, t1, t2 );
    gcry_mpi_gcd(t, t1, t2);
    mpi_fdiv_q(t, phi, t);
    mpi_invm(t, skey->e, t );
    if ( mpi_cmp(t, skey->d ) )
      {
        log_info ( "RSA Oops: d is wrong - fixed\n");
        mpi_set (skey->d, t);
        log_printmpi ("  fixed d", skey->d);
      }

    /* check for correctness of u */
    mpi_invm(t, skey->p, skey->q );
    if ( mpi_cmp(t, skey->u ) )
      {
        log_info ( "RSA Oops: u is wrong - fixed\n");
        mpi_set (skey->u, t);
        log_printmpi ("  fixed u", skey->u);
      }

    log_info ( "RSA secret key check finished\n");

    mpi_free (t);
    mpi_free (t1);
    mpi_free (t2);
    mpi_free (phi);
}
#endif



/* Secret key operation - standard version.
 *
 *	m = c^d mod n
 */


/* Secret key operation - using the CRT.
 *
 *      m1 = c ^ (d mod (p-1)) mod p
 *      m2 = c ^ (d mod (q-1)) mod q
 *      h = u * (m2 - m1) mod q
 *      m = m1 + h * p
 */


/* Secret key operation.
 * Encrypt INPUT with SKEY and put result into
 * OUTPUT.  SKEY has the secret key parameters.
 */




/*********************************************
 **************  interface  ******************
 *********************************************/

#define rsa_generate 0

static gcry_err_code_t
rsa_check_secret_key (gcry_sexp_t keyparms)
{
  gcry_err_code_t rc;
  RSA_secret_key sk = {NULL, NULL, NULL, NULL, NULL, NULL};

  /* To check the key we need the optional parameters. */
  rc = sexp_extract_param (keyparms, NULL, "nedpqu",
                           &sk.n, &sk.e, &sk.d, &sk.p, &sk.q, &sk.u,
                           NULL);
  if (rc)
    goto leave;

  if (!check_secret_key (&sk))
    rc = GPG_ERR_BAD_SECKEY;

 leave:
  _gcry_mpi_release (sk.n);
  _gcry_mpi_release (sk.e);
  _gcry_mpi_release (sk.d);
  _gcry_mpi_release (sk.p);
  _gcry_mpi_release (sk.q);
  _gcry_mpi_release (sk.u);
  if (DBG_CIPHER)
    log_debug ("rsa_testkey    => %s\n", gpg_strerror (rc));
  return rc;
}


static gcry_err_code_t
rsa_encrypt (gcry_sexp_t *r_ciph, gcry_sexp_t s_data, gcry_sexp_t keyparms)
{
  gcry_err_code_t rc;
  struct pk_encoding_ctx ctx;
  gcry_mpi_t data = NULL;
  RSA_public_key pk = {NULL, NULL};
  gcry_mpi_t ciph = NULL;
  unsigned int nbits = rsa_get_nbits (keyparms);

  rc = rsa_check_keysize (nbits);
  if (rc)
    return rc;

  _gcry_pk_util_init_encoding_ctx (&ctx, PUBKEY_OP_ENCRYPT, nbits);

  /* Extract the data.  */
  rc = _gcry_pk_util_data_to_mpi (s_data, &data, &ctx);
  if (rc)
    goto leave;
  if (DBG_CIPHER)
    log_mpidump ("rsa_encrypt data", data);
  if (!data || mpi_is_opaque (data))
    {
      rc = GPG_ERR_INV_DATA;
      goto leave;
    }

  /* Extract the key.  */
  rc = sexp_extract_param (keyparms, NULL, "ne", &pk.n, &pk.e, NULL);
  if (rc)
    goto leave;
  if (DBG_CIPHER)
    {
      log_mpidump ("rsa_encrypt    n", pk.n);
      log_mpidump ("rsa_encrypt    e", pk.e);
    }

  /* Do RSA computation and build result.  */
  ciph = mpi_new (0);
  public (ciph, data, &pk);
  if (DBG_CIPHER)
    log_mpidump ("rsa_encrypt  res", ciph);
  if ((ctx.flags & PUBKEY_FLAG_FIXEDLEN))
    {
      /* We need to make sure to return the correct length to avoid
         problems with missing leading zeroes.  */
      unsigned char *em;
      size_t emlen = (mpi_get_nbits (pk.n)+7)/8;

      rc = _gcry_mpi_to_octet_string (&em, NULL, ciph, emlen);
      if (!rc)
        {
          rc = sexp_build (r_ciph, NULL, "(enc-val(rsa(a%b)))", (int)emlen, em);
          xfree (em);
        }
    }
  else
    rc = sexp_build (r_ciph, NULL, "(enc-val(rsa(a%m)))", ciph);

 leave:
  _gcry_mpi_release (ciph);
  _gcry_mpi_release (pk.n);
  _gcry_mpi_release (pk.e);
  _gcry_mpi_release (data);
  _gcry_pk_util_free_encoding_ctx (&ctx);
  if (DBG_CIPHER)
    log_debug ("rsa_encrypt    => %s\n", gpg_strerror (rc));
  return rc;
}


#define rsa_decrypt 0

#define rsa_sign 0

static gcry_err_code_t
rsa_verify (gcry_sexp_t s_sig, gcry_sexp_t s_data, gcry_sexp_t keyparms)
{
  gcry_err_code_t rc;
  struct pk_encoding_ctx ctx;
  gcry_sexp_t l1 = NULL;
  gcry_mpi_t sig = NULL;
  gcry_mpi_t data = NULL;
  RSA_public_key pk = { NULL, NULL };
  gcry_mpi_t result = NULL;
  unsigned int nbits = rsa_get_nbits (keyparms);

  rc = rsa_check_verify_keysize (nbits);
  if (rc)
    return rc;

  _gcry_pk_util_init_encoding_ctx (&ctx, PUBKEY_OP_VERIFY, nbits);

  /* Extract the data.  */
  rc = _gcry_pk_util_data_to_mpi (s_data, &data, &ctx);
  if (rc)
    goto leave;
  if (DBG_CIPHER)
    log_printmpi ("rsa_verify data", data);
  if (ctx.encoding != PUBKEY_ENC_PSS && mpi_is_opaque (data))
    {
      rc = GPG_ERR_INV_DATA;
      goto leave;
    }

  /* Extract the signature value.  */
  rc = _gcry_pk_util_preparse_sigval (s_sig, rsa_names, &l1, NULL);
  if (rc)
    goto leave;
  rc = sexp_extract_param (l1, NULL, "s", &sig, NULL);
  if (rc)
    goto leave;
  if (DBG_CIPHER)
    log_printmpi ("rsa_verify  sig", sig);

  /* Extract the key.  */
  rc = sexp_extract_param (keyparms, NULL, "ne", &pk.n, &pk.e, NULL);
  if (rc)
    goto leave;
  if (DBG_CIPHER)
    {
      log_printmpi ("rsa_verify    n", pk.n);
      log_printmpi ("rsa_verify    e", pk.e);
    }

  /* Do RSA computation and compare.  */
  result = mpi_new (0);
  public (result, sig, &pk);
  if (DBG_CIPHER)
    log_printmpi ("rsa_verify  cmp", result);
  if (ctx.verify_cmp)
    rc = ctx.verify_cmp (&ctx, result);
  else
    rc = mpi_cmp (result, data) ? GPG_ERR_BAD_SIGNATURE : 0;

 leave:
  _gcry_mpi_release (result);
  _gcry_mpi_release (pk.n);
  _gcry_mpi_release (pk.e);
  _gcry_mpi_release (data);
  _gcry_mpi_release (sig);
  sexp_release (l1);
  _gcry_pk_util_free_encoding_ctx (&ctx);
  if (DBG_CIPHER)
    log_debug ("rsa_verify    => %s\n", rc?gpg_strerror (rc):"Good");
  return rc;
}



/* Return the number of bits for the key described by PARMS.  On error
 * 0 is returned.  The format of PARMS starts with the algorithm name;
 * for example:
 *
 *   (rsa
 *     (n <mpi>)
 *     (e <mpi>))
 *
 * More parameters may be given but we only need N here.
 */
static unsigned int
rsa_get_nbits (gcry_sexp_t parms)
{
  gcry_sexp_t l1;
  gcry_mpi_t n;
  unsigned int nbits;

  l1 = sexp_find_token (parms, "n", 1);
  if (!l1)
    return 0; /* Parameter N not found.  */

  n = sexp_nth_mpi (l1, 1, GCRYMPI_FMT_USG);
  sexp_release (l1);
  nbits = n? mpi_get_nbits (n) : 0;
  _gcry_mpi_release (n);
  return nbits;
}


/* Compute a keygrip.  MD is the hash context which we are going to
   update.  KEYPARAM is an S-expression with the key parameters, this
   is usually a public key but may also be a secret key.  An example
   of such an S-expression is:

      (rsa
        (n #00B...#)
        (e #010001#))

   PKCS-15 says that for RSA only the modulus should be hashed -
   however, it is not clear whether this is meant to use the raw bytes
   (assuming this is an unsigned integer) or whether the DER required
   0 should be prefixed.  We hash the raw bytes.  */
static gpg_err_code_t
compute_keygrip (gcry_md_hd_t md, gcry_sexp_t keyparam)
{
  gcry_sexp_t l1;
  const char *data;
  size_t datalen;

  l1 = sexp_find_token (keyparam, "n", 1);
  if (!l1)
    return GPG_ERR_NO_OBJ;

  data = sexp_nth_data (l1, 1, &datalen);
  if (!data)
    {
      sexp_release (l1);
      return GPG_ERR_NO_OBJ;
    }

  _gcry_md_write (md, data, datalen);
  sexp_release (l1);

  return 0;
}




/*
     Self-test section.
 */





/* Given an S-expression ENCR_DATA of the form:

   (enc-val
    (rsa
     (a a-value)))

   as returned by gcry_pk_decrypt, return the the A-VALUE.  On error,
   return NULL.  */






/* Run a full self-test for ALGO and return 0 on success.  */




gcry_pk_spec_t _gcry_pubkey_spec_rsa =
  {
    GCRY_PK_RSA, { 0, 1 },
    (GCRY_PK_USAGE_SIGN | GCRY_PK_USAGE_ENCR),
    "RSA", rsa_names,
    "ne", "nedpqu", "a", "s", "n",
    rsa_generate,
    rsa_check_secret_key,
    rsa_encrypt,
    rsa_decrypt,
    rsa_sign,
    rsa_verify,
    rsa_get_nbits,
    compute_keygrip
    ,
    GRUB_UTIL_MODNAME("gcry_rsa")
  };


GRUB_MOD_INIT(gcry_rsa)
{
  grub_crypto_pk_rsa = &_gcry_pubkey_spec_rsa;
}

GRUB_MOD_FINI(gcry_rsa)
{
  grub_crypto_pk_rsa = 0;
}
