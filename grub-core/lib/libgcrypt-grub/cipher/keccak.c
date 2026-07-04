/* This file was automatically imported with 
   import_gcry.py. Please don't modify it */
#include <grub/dl.h>
GRUB_MOD_LICENSE ("GPLv3+");
/* keccak.c - SHA3 hash functions
 * Copyright (C) 2015  g10 Code GmbH
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
#include "bithelp.h"
#include "bufhelp.h"
#include "cipher.h"
#include "hash-common.h"



/* USE_64BIT indicates whether to use 64-bit generic implementation.
 * USE_32BIT indicates whether to use 32-bit generic implementation. */
#undef USE_64BIT
#if defined(__x86_64__) || SIZEOF_UNSIGNED_LONG == 8
# define USE_64BIT 1
#else
# define USE_32BIT 1
#endif


/* USE_64BIT_BMI2 indicates whether to compile with 64-bit Intel BMI2 code. */
#undef USE_64BIT_BMI2
#if defined(USE_64BIT) && defined(HAVE_GCC_INLINE_ASM_BMI2) && \
    defined(HAVE_CPU_ARCH_X86)
# define USE_64BIT_BMI2 1
#endif


/* USE_64BIT_SHLD indicates whether to compile with 64-bit Intel SHLD code. */
#undef USE_64BIT_SHLD
#if defined(USE_64BIT) && defined (__GNUC__) && defined(__x86_64__) && \
    defined(HAVE_CPU_ARCH_X86)
# define USE_64BIT_SHLD 1
#endif


/* USE_32BIT_BMI2 indicates whether to compile with 32-bit Intel BMI2 code. */
#undef USE_32BIT_BMI2
#if defined(USE_32BIT) && defined(HAVE_GCC_INLINE_ASM_BMI2) && \
    defined(HAVE_CPU_ARCH_X86)
# define USE_32BIT_BMI2 1
#endif


/* USE_64BIT_AVX512 indicates whether to compile with Intel AVX512 code. */
#undef USE_64BIT_AVX512
#if defined(USE_64BIT) && defined(__x86_64__) && \
    defined(HAVE_GCC_INLINE_ASM_AVX512) && \
    (defined(HAVE_COMPATIBLE_GCC_AMD64_PLATFORM_AS) || \
     defined(HAVE_COMPATIBLE_GCC_WIN64_PLATFORM_AS))
# define USE_64BIT_AVX512 1
#endif


/* USE_64BIT_ARM_NEON indicates whether to enable 64-bit ARM/NEON assembly
 * code. */
#undef USE_64BIT_ARM_NEON
#ifdef ENABLE_NEON_SUPPORT
# if defined(HAVE_ARM_ARCH_V6) && defined(__ARMEL__) \
     && defined(HAVE_COMPATIBLE_GCC_ARM_PLATFORM_AS) \
     && defined(HAVE_GCC_INLINE_ASM_NEON)
#  define USE_64BIT_ARM_NEON 1
# endif
#endif /*ENABLE_NEON_SUPPORT*/


/* USE_S390X_CRYPTO indicates whether to enable zSeries code. */
#undef USE_S390X_CRYPTO
#if defined(HAVE_GCC_INLINE_ASM_S390X)
# define USE_S390X_CRYPTO 1
#endif /* USE_S390X_CRYPTO */


/* x86-64 vector register assembly implementations use SystemV ABI, ABI
 * conversion needed on Win64 through function attribute. */
#undef ASM_FUNC_ABI
#if defined(USE_64BIT_AVX512) && defined(HAVE_COMPATIBLE_GCC_WIN64_PLATFORM_AS)
# define ASM_FUNC_ABI __attribute__((sysv_abi))
#else
# define ASM_FUNC_ABI
#endif


#if defined(USE_64BIT) || defined(USE_64BIT_ARM_NEON)
# define NEED_COMMON64 1
#endif

#ifdef USE_32BIT
# define NEED_COMMON32BI 1
#endif


#define SHA3_DELIMITED_SUFFIX 0x06
#define SHAKE_DELIMITED_SUFFIX 0x1F
#define CSHAKE_DELIMITED_SUFFIX 0x04

typedef struct
{
  union {
#ifdef NEED_COMMON64
    u64 state64[25];
#endif
#ifdef NEED_COMMON32BI
    u32 state32bi[50];
#endif
  } u;
} KECCAK_STATE;


typedef struct
{
  unsigned int (*permute)(KECCAK_STATE *hd);
  unsigned int (*absorb)(KECCAK_STATE *hd, int pos, const byte *lanes,
			 size_t nlanes, int blocklanes);
  unsigned int (*extract) (KECCAK_STATE *hd, unsigned int pos, byte *outbuf,
			   unsigned int outlen);
} keccak_ops_t;


typedef struct KECCAK_CONTEXT_S
{
  KECCAK_STATE state;
  unsigned int outlen;
  unsigned int blocksize;
  unsigned int count;
  unsigned int suffix:8;
  unsigned int shake_in_extract_mode:1;
  unsigned int shake_in_read_mode:1;
  const keccak_ops_t *ops;
#ifdef USE_S390X_CRYPTO
  unsigned int kimd_func;
  unsigned int buf_pos;
  byte buf[1344 / 8]; /* SHAKE128 requires biggest buffer, 1344 bits. */
#endif
} KECCAK_CONTEXT;



#ifdef NEED_COMMON64

const u64 _gcry_keccak_round_consts_64bit[24 + 1] =
{
  U64_C(0x0000000000000001), U64_C(0x0000000000008082),
  U64_C(0x800000000000808A), U64_C(0x8000000080008000),
  U64_C(0x000000000000808B), U64_C(0x0000000080000001),
  U64_C(0x8000000080008081), U64_C(0x8000000000008009),
  U64_C(0x000000000000008A), U64_C(0x0000000000000088),
  U64_C(0x0000000080008009), U64_C(0x000000008000000A),
  U64_C(0x000000008000808B), U64_C(0x800000000000008B),
  U64_C(0x8000000000008089), U64_C(0x8000000000008003),
  U64_C(0x8000000000008002), U64_C(0x8000000000000080),
  U64_C(0x000000000000800A), U64_C(0x800000008000000A),
  U64_C(0x8000000080008081), U64_C(0x8000000000008080),
  U64_C(0x0000000080000001), U64_C(0x8000000080008008),
  U64_C(0xFFFFFFFFFFFFFFFF)
};

static unsigned int
keccak_extract64(KECCAK_STATE *hd, unsigned int pos, byte *outbuf,
		 unsigned int outlen)
{
  unsigned int i;

  /* NOTE: when pos == 0, hd and outbuf may point to same memory (SHA-3). */

  for (i = pos; i < pos + outlen / 8 + !!(outlen % 8); i++)
    {
      u64 tmp = hd->u.state64[i];
      buf_put_le64(outbuf, tmp);
      outbuf += 8;
    }

  return 0;
}

#endif /* NEED_COMMON64 */


#ifdef NEED_COMMON32BI

static const u32 round_consts_32bit[2 * 24] =
{
  0x00000001UL, 0x00000000UL, 0x00000000UL, 0x00000089UL,
  0x00000000UL, 0x8000008bUL, 0x00000000UL, 0x80008080UL,
  0x00000001UL, 0x0000008bUL, 0x00000001UL, 0x00008000UL,
  0x00000001UL, 0x80008088UL, 0x00000001UL, 0x80000082UL,
  0x00000000UL, 0x0000000bUL, 0x00000000UL, 0x0000000aUL,
  0x00000001UL, 0x00008082UL, 0x00000000UL, 0x00008003UL,
  0x00000001UL, 0x0000808bUL, 0x00000001UL, 0x8000000bUL,
  0x00000001UL, 0x8000008aUL, 0x00000001UL, 0x80000081UL,
  0x00000000UL, 0x80000081UL, 0x00000000UL, 0x80000008UL,
  0x00000000UL, 0x00000083UL, 0x00000000UL, 0x80008003UL,
  0x00000001UL, 0x80008088UL, 0x00000000UL, 0x80000088UL,
  0x00000001UL, 0x00008000UL, 0x00000000UL, 0x80008082UL
};

static unsigned int
keccak_extract32bi(KECCAK_STATE *hd, unsigned int pos, byte *outbuf,
		   unsigned int outlen)
{
  unsigned int i;
  u32 x0;
  u32 x1;
  u32 t;

  /* NOTE: when pos == 0, hd and outbuf may point to same memory (SHA-3). */

  for (i = pos; i < pos + outlen / 8 + !!(outlen % 8); i++)
    {
      x0 = hd->u.state32bi[i * 2 + 0];
      x1 = hd->u.state32bi[i * 2 + 1];

      t = (x0 & 0x0000FFFFUL) + (x1 << 16);
      x1 = (x0 >> 16) + (x1 & 0xFFFF0000UL);
      x0 = t;
      t = (x0 ^ (x0 >> 8)) & 0x0000FF00UL; x0 = x0 ^ t ^ (t << 8);
      t = (x0 ^ (x0 >> 4)) & 0x00F000F0UL; x0 = x0 ^ t ^ (t << 4);
      t = (x0 ^ (x0 >> 2)) & 0x0C0C0C0CUL; x0 = x0 ^ t ^ (t << 2);
      t = (x0 ^ (x0 >> 1)) & 0x22222222UL; x0 = x0 ^ t ^ (t << 1);
      t = (x1 ^ (x1 >> 8)) & 0x0000FF00UL; x1 = x1 ^ t ^ (t << 8);
      t = (x1 ^ (x1 >> 4)) & 0x00F000F0UL; x1 = x1 ^ t ^ (t << 4);
      t = (x1 ^ (x1 >> 2)) & 0x0C0C0C0CUL; x1 = x1 ^ t ^ (t << 2);
      t = (x1 ^ (x1 >> 1)) & 0x22222222UL; x1 = x1 ^ t ^ (t << 1);

      buf_put_le32(&outbuf[0], x0);
      buf_put_le32(&outbuf[4], x1);
      outbuf += 8;
    }

  return 0;
}

static inline void
keccak_absorb_lane32bi(u32 *lane, u32 x0, u32 x1)
{
  u32 t;

  t = (x0 ^ (x0 >> 1)) & 0x22222222UL; x0 = x0 ^ t ^ (t << 1);
  t = (x0 ^ (x0 >> 2)) & 0x0C0C0C0CUL; x0 = x0 ^ t ^ (t << 2);
  t = (x0 ^ (x0 >> 4)) & 0x00F000F0UL; x0 = x0 ^ t ^ (t << 4);
  t = (x0 ^ (x0 >> 8)) & 0x0000FF00UL; x0 = x0 ^ t ^ (t << 8);
  t = (x1 ^ (x1 >> 1)) & 0x22222222UL; x1 = x1 ^ t ^ (t << 1);
  t = (x1 ^ (x1 >> 2)) & 0x0C0C0C0CUL; x1 = x1 ^ t ^ (t << 2);
  t = (x1 ^ (x1 >> 4)) & 0x00F000F0UL; x1 = x1 ^ t ^ (t << 4);
  t = (x1 ^ (x1 >> 8)) & 0x0000FF00UL; x1 = x1 ^ t ^ (t << 8);
  lane[0] ^= (x0 & 0x0000FFFFUL) + (x1 << 16);
  lane[1] ^= (x0 >> 16) + (x1 & 0xFFFF0000UL);
}

#endif /* NEED_COMMON32BI */


/* Construct generic 64-bit implementation. */
#ifdef USE_64BIT

#if __GNUC__ >= 4 && defined(__x86_64__) && 0

static inline void absorb_lanes64_8(u64 *dst, const byte *in)
{
  asm ("movdqu 0*16(%[dst]), %%xmm0\n\t"
       "movdqu 0*16(%[in]), %%xmm4\n\t"
       "movdqu 1*16(%[dst]), %%xmm1\n\t"
       "movdqu 1*16(%[in]), %%xmm5\n\t"
       "movdqu 2*16(%[dst]), %%xmm2\n\t"
       "movdqu 3*16(%[dst]), %%xmm3\n\t"
       "pxor %%xmm4, %%xmm0\n\t"
       "pxor %%xmm5, %%xmm1\n\t"
       "movdqu 2*16(%[in]), %%xmm4\n\t"
       "movdqu 3*16(%[in]), %%xmm5\n\t"
       "movdqu %%xmm0, 0*16(%[dst])\n\t"
       "pxor %%xmm4, %%xmm2\n\t"
       "movdqu %%xmm1, 1*16(%[dst])\n\t"
       "pxor %%xmm5, %%xmm3\n\t"
       "movdqu %%xmm2, 2*16(%[dst])\n\t"
       "movdqu %%xmm3, 3*16(%[dst])\n\t"
       :
       : [dst] "r" (dst), [in] "r" (in)
       : "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "memory");
}

static inline void absorb_lanes64_4(u64 *dst, const byte *in)
{
  asm ("movdqu 0*16(%[dst]), %%xmm0\n\t"
       "movdqu 0*16(%[in]), %%xmm4\n\t"
       "movdqu 1*16(%[dst]), %%xmm1\n\t"
       "movdqu 1*16(%[in]), %%xmm5\n\t"
       "pxor %%xmm4, %%xmm0\n\t"
       "pxor %%xmm5, %%xmm1\n\t"
       "movdqu %%xmm0, 0*16(%[dst])\n\t"
       "movdqu %%xmm1, 1*16(%[dst])\n\t"
       :
       : [dst] "r" (dst), [in] "r" (in)
       : "xmm0", "xmm1", "xmm4", "xmm5", "memory");
}

static inline void absorb_lanes64_2(u64 *dst, const byte *in)
{
  asm ("movdqu 0*16(%[dst]), %%xmm0\n\t"
       "movdqu 0*16(%[in]), %%xmm4\n\t"
       "pxor %%xmm4, %%xmm0\n\t"
       "movdqu %%xmm0, 0*16(%[dst])\n\t"
       :
       : [dst] "r" (dst), [in] "r" (in)
       : "xmm0", "xmm4", "memory");
}

#else /* __x86_64__ */

static inline void absorb_lanes64_8(u64 *dst, const byte *in)
{
  dst[0] ^= buf_get_le64(in + 8 * 0);
  dst[1] ^= buf_get_le64(in + 8 * 1);
  dst[2] ^= buf_get_le64(in + 8 * 2);
  dst[3] ^= buf_get_le64(in + 8 * 3);
  dst[4] ^= buf_get_le64(in + 8 * 4);
  dst[5] ^= buf_get_le64(in + 8 * 5);
  dst[6] ^= buf_get_le64(in + 8 * 6);
  dst[7] ^= buf_get_le64(in + 8 * 7);
}

static inline void absorb_lanes64_4(u64 *dst, const byte *in)
{
  dst[0] ^= buf_get_le64(in + 8 * 0);
  dst[1] ^= buf_get_le64(in + 8 * 1);
  dst[2] ^= buf_get_le64(in + 8 * 2);
  dst[3] ^= buf_get_le64(in + 8 * 3);
}

static inline void absorb_lanes64_2(u64 *dst, const byte *in)
{
  dst[0] ^= buf_get_le64(in + 8 * 0);
  dst[1] ^= buf_get_le64(in + 8 * 1);
}

#endif /* !__x86_64__ */

static inline void absorb_lanes64_1(u64 *dst, const byte *in)
{
  dst[0] ^= buf_get_le64(in + 8 * 0);
}


# define ANDN64(x, y) (~(x) & (y))
# define ROL64(x, n) (((x) << ((unsigned int)n & 63)) | \
		      ((x) >> ((64 - (unsigned int)(n)) & 63)))

# define KECCAK_F1600_PERMUTE_FUNC_NAME keccak_f1600_state_permute64
# define KECCAK_F1600_ABSORB_FUNC_NAME keccak_absorb_lanes64
# include "keccak_permute_64.h"

# undef ANDN64
# undef ROL64
# undef KECCAK_F1600_PERMUTE_FUNC_NAME
# undef KECCAK_F1600_ABSORB_FUNC_NAME

static const keccak_ops_t keccak_generic64_ops =
{
  .permute = keccak_f1600_state_permute64,
  .absorb = keccak_absorb_lanes64,
  .extract = keccak_extract64,
};

#endif /* USE_64BIT */


/* Construct 64-bit Intel SHLD implementation. */
#ifdef USE_64BIT_SHLD

# define ANDN64(x, y) (~(x) & (y))
# define ROL64(x, n) ({ \
			u64 tmp = (x); \
			asm ("shldq %1, %0, %0" \
			     : "+r" (tmp) \
			     : "J" ((n) & 63) \
			     : "cc"); \
			tmp; })

# define KECCAK_F1600_PERMUTE_FUNC_NAME keccak_f1600_state_permute64_shld
# define KECCAK_F1600_ABSORB_FUNC_NAME keccak_absorb_lanes64_shld
# include "keccak_permute_64.h"

# undef ANDN64
# undef ROL64
# undef KECCAK_F1600_PERMUTE_FUNC_NAME
# undef KECCAK_F1600_ABSORB_FUNC_NAME

static const keccak_ops_t keccak_shld_64_ops =
{
  .permute = keccak_f1600_state_permute64_shld,
  .absorb = keccak_absorb_lanes64_shld,
  .extract = keccak_extract64,
};

#endif /* USE_64BIT_SHLD */


/* Construct 64-bit Intel BMI2 implementation. */
#ifdef USE_64BIT_BMI2

# define ANDN64(x, y) ({ \
			u64 tmp; \
			asm ("andnq %2, %1, %0" \
			     : "=r" (tmp) \
			     : "r0" (x), "rm" (y)); \
			tmp; })

# define ROL64(x, n) ({ \
			u64 tmp; \
			asm ("rorxq %2, %1, %0" \
			     : "=r" (tmp) \
			     : "rm0" (x), "J" (64 - ((n) & 63))); \
			tmp; })

# define KECCAK_F1600_PERMUTE_FUNC_NAME keccak_f1600_state_permute64_bmi2
# define KECCAK_F1600_ABSORB_FUNC_NAME keccak_absorb_lanes64_bmi2
# include "keccak_permute_64.h"

# undef ANDN64
# undef ROL64
# undef KECCAK_F1600_PERMUTE_FUNC_NAME
# undef KECCAK_F1600_ABSORB_FUNC_NAME

static const keccak_ops_t keccak_bmi2_64_ops =
{
  .permute = keccak_f1600_state_permute64_bmi2,
  .absorb = keccak_absorb_lanes64_bmi2,
  .extract = keccak_extract64,
};

#endif /* USE_64BIT_BMI2 */


/* 64-bit Intel AVX512 implementation. */
#ifdef USE_64BIT_AVX512

extern ASM_FUNC_ABI unsigned int
_gcry_keccak_f1600_state_permute64_avx512(u64 *state, const u64 *rconst);

extern ASM_FUNC_ABI unsigned int
_gcry_keccak_absorb_blocks_avx512(u64 *state, const u64 *rconst,
                                  const byte *lanes, u64 nlanes,
                                  u64 blocklanes, u64 *new_lanes);

static unsigned int
keccak_f1600_state_permute64_avx512(KECCAK_STATE *hd)
{
  return _gcry_keccak_f1600_state_permute64_avx512 (
                                hd->u.state64, _gcry_keccak_round_consts_64bit);
}

static unsigned int
keccak_absorb_lanes64_avx512(KECCAK_STATE *hd, int pos, const byte *lanes,
			     size_t nlanes, int blocklanes)
{
  while (nlanes)
    {
      if (pos == 0 && blocklanes > 0 && nlanes >= (size_t)blocklanes)
        {
          /* Get new pointer through u64 variable for "x32" compatibility. */
          u64 new_lanes;
          nlanes = _gcry_keccak_absorb_blocks_avx512 (
                            hd->u.state64, _gcry_keccak_round_consts_64bit,
                            lanes, nlanes, blocklanes, &new_lanes);
          lanes = (const byte *)(uintptr_t)new_lanes;
        }

      while (nlanes)
	{
	  hd->u.state64[pos] ^= buf_get_le64 (lanes);
	  lanes += 8;
	  nlanes--;

	  if (++pos == blocklanes)
	    {
	      keccak_f1600_state_permute64_avx512 (hd);
	      pos = 0;
	      break;
	    }
	}
    }

  return 0;
}

static const keccak_ops_t keccak_avx512_64_ops =
{
  .permute = keccak_f1600_state_permute64_avx512,
  .absorb = keccak_absorb_lanes64_avx512,
  .extract = keccak_extract64,
};

#endif /* USE_64BIT_AVX512 */


/* 64-bit ARMv7/NEON implementation. */
#ifdef USE_64BIT_ARM_NEON

unsigned int _gcry_keccak_permute_armv7_neon(u64 *state);
unsigned int _gcry_keccak_absorb_lanes64_armv7_neon(u64 *state, int pos,
						    const byte *lanes,
						    size_t nlanes,
						    int blocklanes);

static unsigned int keccak_permute64_armv7_neon(KECCAK_STATE *hd)
{
  return _gcry_keccak_permute_armv7_neon(hd->u.state64);
}

static unsigned int
keccak_absorb_lanes64_armv7_neon(KECCAK_STATE *hd, int pos, const byte *lanes,
				 size_t nlanes, int blocklanes)
{
  if (blocklanes < 0)
    {
      /* blocklanes == -1, permutationless absorb from keccak_final. */

      while (nlanes)
	{
	  hd->u.state64[pos] ^= buf_get_le64(lanes);
	  lanes += 8;
	  nlanes--;
	}

      return 0;
    }
  else
    {
      return _gcry_keccak_absorb_lanes64_armv7_neon(hd->u.state64, pos, lanes,
						    nlanes, blocklanes);
    }
}

static const keccak_ops_t keccak_armv7_neon_64_ops =
{
  .permute = keccak_permute64_armv7_neon,
  .absorb = keccak_absorb_lanes64_armv7_neon,
  .extract = keccak_extract64,
};

#endif /* USE_64BIT_ARM_NEON */


/* Construct generic 32-bit implementation. */
#ifdef USE_32BIT

# define ANDN32(x, y) (~(x) & (y))
# define ROL32(x, n) (((x) << ((unsigned int)n & 31)) | \
		      ((x) >> ((32 - (unsigned int)(n)) & 31)))

# define KECCAK_F1600_PERMUTE_FUNC_NAME keccak_f1600_state_permute32bi
# include "keccak_permute_32.h"

# undef ANDN32
# undef ROL32
# undef KECCAK_F1600_PERMUTE_FUNC_NAME

static unsigned int
keccak_absorb_lanes32bi(KECCAK_STATE *hd, int pos, const byte *lanes,
		        size_t nlanes, int blocklanes)
{
  unsigned int burn = 0;

  while (nlanes)
    {
      keccak_absorb_lane32bi(&hd->u.state32bi[pos * 2],
			     buf_get_le32(lanes + 0),
			     buf_get_le32(lanes + 4));
      lanes += 8;
      nlanes--;

      if (++pos == blocklanes)
	{
	  burn = keccak_f1600_state_permute32bi(hd);
	  pos = 0;
	}
    }

  return burn;
}

static const keccak_ops_t keccak_generic32bi_ops =
{
  .permute = keccak_f1600_state_permute32bi,
  .absorb = keccak_absorb_lanes32bi,
  .extract = keccak_extract32bi,
};

#endif /* USE_32BIT */


/* Construct 32-bit Intel BMI2 implementation. */
#ifdef USE_32BIT_BMI2

# define ANDN32(x, y) ({ \
			u32 tmp; \
			asm ("andnl %2, %1, %0" \
			     : "=r" (tmp) \
			     : "r0" (x), "rm" (y)); \
			tmp; })

# define ROL32(x, n) ({ \
			u32 tmp; \
			asm ("rorxl %2, %1, %0" \
			     : "=r" (tmp) \
			     : "rm0" (x), "J" (32 - ((n) & 31))); \
			tmp; })

# define KECCAK_F1600_PERMUTE_FUNC_NAME keccak_f1600_state_permute32bi_bmi2
# include "keccak_permute_32.h"

# undef ANDN32
# undef ROL32
# undef KECCAK_F1600_PERMUTE_FUNC_NAME

static inline u32 pext(u32 x, u32 mask)
{
  u32 tmp;
  asm ("pextl %2, %1, %0" : "=r" (tmp) : "r0" (x), "rm" (mask));
  return tmp;
}

static inline u32 pdep(u32 x, u32 mask)
{
  u32 tmp;
  asm ("pdepl %2, %1, %0" : "=r" (tmp) : "r0" (x), "rm" (mask));
  return tmp;
}

static inline void
keccak_absorb_lane32bi_bmi2(u32 *lane, u32 x0, u32 x1)
{
  x0 = pdep(pext(x0, 0x55555555), 0x0000ffff) | (pext(x0, 0xaaaaaaaa) << 16);
  x1 = pdep(pext(x1, 0x55555555), 0x0000ffff) | (pext(x1, 0xaaaaaaaa) << 16);

  lane[0] ^= (x0 & 0x0000FFFFUL) + (x1 << 16);
  lane[1] ^= (x0 >> 16) + (x1 & 0xFFFF0000UL);
}

static unsigned int
keccak_absorb_lanes32bi_bmi2(KECCAK_STATE *hd, int pos, const byte *lanes,
		             size_t nlanes, int blocklanes)
{
  unsigned int burn = 0;

  while (nlanes)
    {
      keccak_absorb_lane32bi_bmi2(&hd->u.state32bi[pos * 2],
			          buf_get_le32(lanes + 0),
			          buf_get_le32(lanes + 4));
      lanes += 8;
      nlanes--;

      if (++pos == blocklanes)
	{
	  burn = keccak_f1600_state_permute32bi_bmi2(hd);
	  pos = 0;
	}
    }

  return burn;
}

static unsigned int
keccak_extract32bi_bmi2(KECCAK_STATE *hd, unsigned int pos, byte *outbuf,
			unsigned int outlen)
{
  unsigned int i;
  u32 x0;
  u32 x1;
  u32 t;

  /* NOTE: when pos == 0, hd and outbuf may point to same memory (SHA-3). */

  for (i = pos; i < pos + outlen / 8 + !!(outlen % 8); i++)
    {
      x0 = hd->u.state32bi[i * 2 + 0];
      x1 = hd->u.state32bi[i * 2 + 1];

      t = (x0 & 0x0000FFFFUL) + (x1 << 16);
      x1 = (x0 >> 16) + (x1 & 0xFFFF0000UL);
      x0 = t;

      x0 = pdep(pext(x0, 0xffff0001), 0xaaaaaaab) | pdep(x0 >> 1, 0x55555554);
      x1 = pdep(pext(x1, 0xffff0001), 0xaaaaaaab) | pdep(x1 >> 1, 0x55555554);

      buf_put_le32(&outbuf[0], x0);
      buf_put_le32(&outbuf[4], x1);
      outbuf += 8;
    }

  return 0;
}

static const keccak_ops_t keccak_bmi2_32bi_ops =
{
  .permute = keccak_f1600_state_permute32bi_bmi2,
  .absorb = keccak_absorb_lanes32bi_bmi2,
  .extract = keccak_extract32bi_bmi2,
};

#endif /* USE_32BIT_BMI2 */


#ifdef USE_S390X_CRYPTO
#include "asm-inline-s390x.h"

static inline void
keccak_bwrite_s390x (void *context, const byte *in, size_t inlen)
{
  KECCAK_CONTEXT *ctx = context;

  /* Write full-blocks. */
  kimd_execute (ctx->kimd_func, &ctx->state, in, inlen);
  return;
}

static inline void
keccak_final_s390x (void *context)
{
  KECCAK_CONTEXT *ctx = context;

  if (ctx->suffix == SHA3_DELIMITED_SUFFIX)
    {
      klmd_execute (ctx->kimd_func, &ctx->state, ctx->buf, ctx->count);
    }
  else
    {
      klmd_shake_execute (ctx->kimd_func, &ctx->state, NULL, 0, ctx->buf,
			  ctx->count);
      ctx->count = 0;
      ctx->buf_pos = 0;
    }

  return;
}

static inline void
keccak_bextract_s390x (void *context, byte *out, size_t outlen)
{
  KECCAK_CONTEXT *ctx = context;

  /* Extract full-blocks. */
  klmd_shake_execute (ctx->kimd_func | KLMD_PADDING_STATE, &ctx->state,
		      out, outlen, NULL, 0);
  return;
}

static void
keccak_write_s390x (void *context, const byte *inbuf, size_t inlen)
{
  KECCAK_CONTEXT *hd = context;
  const size_t blocksize = hd->blocksize;
  size_t inblocks;
  size_t copylen;

  while (hd->count)
    {
      if (hd->count == blocksize)  /* Flush the buffer. */
	{
	  keccak_bwrite_s390x (hd, hd->buf, blocksize);
	  hd->count = 0;
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
      inblocks = inlen / blocksize;
      keccak_bwrite_s390x (hd, inbuf, inblocks * blocksize);
      hd->count = 0;
      inlen -= inblocks * blocksize;
      inbuf += inblocks * blocksize;
    }

  if (inlen)
    {
      buf_cpy (hd->buf, inbuf, inlen);
      hd->count = inlen;
    }
}

static void
keccak_extract_s390x (void *context, void *outbuf_arg, size_t outlen)
{
  KECCAK_CONTEXT *hd = context;
  const size_t blocksize = hd->blocksize;
  byte *outbuf = outbuf_arg;

  while (outlen)
    {
      gcry_assert(hd->count == 0 || hd->buf_pos < hd->count);

      if (hd->buf_pos < hd->count && outlen)
	{
	  size_t copylen = hd->count - hd->buf_pos;

	  if (copylen > outlen)
	    copylen = outlen;

	  buf_cpy (outbuf, &hd->buf[hd->buf_pos], copylen);

	  outbuf += copylen;
	  outlen -= copylen;
	  hd->buf_pos += copylen;
	}

      if (hd->buf_pos == hd->count)
	{
	  hd->buf_pos = 0;
	  hd->count = 0;
	}

      if (outlen == 0)
	return;

      if (outlen >= blocksize)
	{
	  size_t outblocks = outlen / blocksize;

	  keccak_bextract_s390x (context, outbuf, outblocks * blocksize);

	  outlen -= outblocks * blocksize;
	  outbuf += outblocks * blocksize;

	  if (outlen == 0)
	    return;
	}

      keccak_bextract_s390x (context, hd->buf, blocksize);
      hd->count = blocksize;
    }
}
#endif /* USE_S390X_CRYPTO */


static void
keccak_write (void *context, const void *inbuf_arg, size_t inlen)
{
  KECCAK_CONTEXT *ctx = context;
  const size_t bsize = ctx->blocksize;
  const size_t blocklanes = bsize / 8;
  const byte *inbuf = inbuf_arg;
  unsigned int nburn, burn = 0;
  unsigned int count, i;
  unsigned int pos;
  size_t nlanes;

#ifdef USE_S390X_CRYPTO
  if (ctx->kimd_func)
    {
      keccak_write_s390x (context, inbuf, inlen);
      return;
    }
#endif

  count = ctx->count;

  if (inlen && (count % 8))
    {
      byte lane[8] = { 0, };

      /* Complete absorbing partial input lane. */

      pos = count / 8;

      for (i = count % 8; inlen && i < 8; i++)
	{
	  lane[i] = *inbuf++;
	  inlen--;
	  count++;
	}

      if (count == bsize)
	count = 0;

      nburn = ctx->ops->absorb(&ctx->state, pos, lane, 1,
			       (count % 8) ? -1 : blocklanes);
      burn = nburn > burn ? nburn : burn;
    }

  /* Absorb full input lanes. */

  pos = count / 8;
  nlanes = inlen / 8;
  if (nlanes > 0)
    {
      nburn = ctx->ops->absorb(&ctx->state, pos, inbuf, nlanes, blocklanes);
      burn = nburn > burn ? nburn : burn;
      inlen -= nlanes * 8;
      inbuf += nlanes * 8;
      count = ((size_t) count + nlanes * 8) % bsize;
    }

  if (inlen)
    {
      byte lane[8] = { 0, };

      /* Absorb remaining partial input lane. */

      pos = count / 8;

      for (i = count % 8; inlen && i < 8; i++)
	{
	  lane[i] = *inbuf++;
	  inlen--;
	  count++;
	}

      nburn = ctx->ops->absorb(&ctx->state, pos, lane, 1, -1);
      burn = nburn > burn ? nburn : burn;

      gcry_assert(count < bsize);
    }

  ctx->count = count;

  if (burn)
    _gcry_burn_stack (burn);
}


static void
keccak_init (int algo, void *context, unsigned int flags)
{
  KECCAK_CONTEXT *ctx = context;
  KECCAK_STATE *hd = &ctx->state;
  unsigned int features = _gcry_get_hw_features ();

  (void)flags;
  (void)features;

  memset (hd, 0, sizeof *hd);

  ctx->count = 0;
  ctx->shake_in_extract_mode = 0;
  ctx->shake_in_read_mode = 0;

  /* Select generic implementation. */
#ifdef USE_64BIT
  ctx->ops = &keccak_generic64_ops;
#elif defined USE_32BIT
  ctx->ops = &keccak_generic32bi_ops;
#endif

  /* Select optimized implementation based in hw features. */
  if (0) {}
#ifdef USE_64BIT_AVX512
  else if (features & HWF_INTEL_AVX512)
    ctx->ops = &keccak_avx512_64_ops;
#endif
#ifdef USE_64BIT_ARM_NEON
  else if (features & HWF_ARM_NEON)
    ctx->ops = &keccak_armv7_neon_64_ops;
#endif
#ifdef USE_64BIT_BMI2
  else if (features & HWF_INTEL_BMI2)
    ctx->ops = &keccak_bmi2_64_ops;
#endif
#ifdef USE_32BIT_BMI2
  else if (features & HWF_INTEL_BMI2)
    ctx->ops = &keccak_bmi2_32bi_ops;
#endif
#ifdef USE_64BIT_SHLD
  else if (features & HWF_INTEL_FAST_SHLD)
    ctx->ops = &keccak_shld_64_ops;
#endif

  /* Set input block size, in Keccak terms this is called 'rate'. */

  switch (algo)
    {
    case GCRY_MD_SHA3_224:
      ctx->suffix = SHA3_DELIMITED_SUFFIX;
      ctx->blocksize = 1152 / 8;
      ctx->outlen = 224 / 8;
      break;
    case GCRY_MD_SHA3_256:
      ctx->suffix = SHA3_DELIMITED_SUFFIX;
      ctx->blocksize = 1088 / 8;
      ctx->outlen = 256 / 8;
      break;
    case GCRY_MD_SHA3_384:
      ctx->suffix = SHA3_DELIMITED_SUFFIX;
      ctx->blocksize = 832 / 8;
      ctx->outlen = 384 / 8;
      break;
    case GCRY_MD_SHA3_512:
      ctx->suffix = SHA3_DELIMITED_SUFFIX;
      ctx->blocksize = 576 / 8;
      ctx->outlen = 512 / 8;
      break;
    case GCRY_MD_CSHAKE128:
    case GCRY_MD_SHAKE128:
      ctx->suffix = SHAKE_DELIMITED_SUFFIX;
      ctx->blocksize = 1344 / 8;
      ctx->outlen = 256 / 8;
      break;
    case GCRY_MD_CSHAKE256:
    case GCRY_MD_SHAKE256:
      ctx->suffix = SHAKE_DELIMITED_SUFFIX;
      ctx->blocksize = 1088 / 8;
      ctx->outlen = 512 / 8;
      break;
    default:
      BUG();
    }

#ifdef USE_S390X_CRYPTO
  ctx->kimd_func = 0;
  if ((features & HWF_S390X_MSA) != 0)
    {
      unsigned int kimd_func = 0;

      switch (algo)
	{
	case GCRY_MD_SHA3_224:
	  kimd_func = KMID_FUNCTION_SHA3_224;
	  break;
	case GCRY_MD_SHA3_256:
	  kimd_func = KMID_FUNCTION_SHA3_256;
	  break;
	case GCRY_MD_SHA3_384:
	  kimd_func = KMID_FUNCTION_SHA3_384;
	  break;
	case GCRY_MD_SHA3_512:
	  kimd_func = KMID_FUNCTION_SHA3_512;
	  break;
	case GCRY_MD_CSHAKE128:
	case GCRY_MD_SHAKE128:
	  kimd_func = KMID_FUNCTION_SHAKE128;
	  break;
	case GCRY_MD_CSHAKE256:
	case GCRY_MD_SHAKE256:
	  kimd_func = KMID_FUNCTION_SHAKE256;
	  break;
	}

      if ((kimd_query () & km_function_to_mask (kimd_func)) &&
	  (klmd_query () & km_function_to_mask (kimd_func)))
	{
	  ctx->kimd_func = kimd_func;
	}
    }
#endif
}

static void
sha3_224_init (void *context, unsigned int flags)
{
  keccak_init (GCRY_MD_SHA3_224, context, flags);
}

static void
sha3_256_init (void *context, unsigned int flags)
{
  keccak_init (GCRY_MD_SHA3_256, context, flags);
}

static void
sha3_384_init (void *context, unsigned int flags)
{
  keccak_init (GCRY_MD_SHA3_384, context, flags);
}

static void
sha3_512_init (void *context, unsigned int flags)
{
  keccak_init (GCRY_MD_SHA3_512, context, flags);
}

static void
shake128_init (void *context, unsigned int flags)
{
  keccak_init (GCRY_MD_SHAKE128, context, flags);
}

static void
shake256_init (void *context, unsigned int flags)
{
  keccak_init (GCRY_MD_SHAKE256, context, flags);
}

/* The routine final terminates the computation and
 * returns the digest.
 * The handle is prepared for a new cycle, but adding bytes to the
 * handle will the destroy the returned buffer.
 * Returns: 64 bytes representing the digest.  When used for sha384,
 * we take the leftmost 48 of those bytes.
 */
static void
keccak_final (void *context)
{
  KECCAK_CONTEXT *ctx = context;
  KECCAK_STATE *hd = &ctx->state;
  const size_t bsize = ctx->blocksize;
  const byte suffix = ctx->suffix;
  unsigned int nburn, burn = 0;
  unsigned int lastbytes;
  byte lane[8];

#ifdef USE_S390X_CRYPTO
  if (ctx->kimd_func)
    {
      keccak_final_s390x (context);
      return;
    }
#endif

  lastbytes = ctx->count;

  /* Do the padding and switch to the squeezing phase */

  /* Absorb the last few bits and add the first bit of padding (which
     coincides with the delimiter in delimited suffix) */
  buf_put_le64(lane, (u64)suffix << ((lastbytes % 8) * 8));
  nburn = ctx->ops->absorb(&ctx->state, lastbytes / 8, lane, 1, -1);
  burn = nburn > burn ? nburn : burn;

  /* Add the second bit of padding. */
  buf_put_le64(lane, (u64)0x80 << (((bsize - 1) % 8) * 8));
  nburn = ctx->ops->absorb(&ctx->state, (bsize - 1) / 8, lane, 1, -1);
  burn = nburn > burn ? nburn : burn;

  if (suffix == SHA3_DELIMITED_SUFFIX)
    {
      /* Switch to the squeezing phase. */
      nburn = ctx->ops->permute(hd);
      burn = nburn > burn ? nburn : burn;

      /* Squeeze out the SHA3 digest. */
      nburn = ctx->ops->extract(hd, 0, (void *)hd, ctx->outlen);
      burn = nburn > burn ? nburn : burn;
    }
  else
    {
      /* Output for SHAKE can now be read with md_extract(). */

      ctx->count = 0;
    }

  wipememory(lane, sizeof(lane));
  if (burn)
    _gcry_burn_stack (burn);
}


static byte *
keccak_read (void *context)
{
  KECCAK_CONTEXT *ctx = (KECCAK_CONTEXT *) context;
  KECCAK_STATE *hd = &ctx->state;
  return (byte *)&hd->u;
}


static gcry_err_code_t
do_keccak_extract (void *context, void *out, size_t outlen)
{
  KECCAK_CONTEXT *ctx = context;
  KECCAK_STATE *hd = &ctx->state;
  const size_t bsize = ctx->blocksize;
  unsigned int nburn, burn = 0;
  byte *outbuf = out;
  unsigned int nlanes;
  unsigned int nleft;
  unsigned int count;
  unsigned int i;
  byte lane[8];

#ifdef USE_S390X_CRYPTO
  if (ctx->kimd_func)
    {
      keccak_extract_s390x (context, out, outlen);
      return 0;
    }
#endif

  count = ctx->count;

  while (count && outlen && (outlen < 8 || count % 8))
    {
      /* Extract partial lane. */
      nburn = ctx->ops->extract(hd, count / 8, lane, 8);
      burn = nburn > burn ? nburn : burn;

      for (i = count % 8; outlen && i < 8; i++)
	{
	  *outbuf++ = lane[i];
	  outlen--;
	  count++;
	}

      gcry_assert(count <= bsize);

      if (count == bsize)
	count = 0;
    }

  if (outlen >= 8 && count)
    {
      /* Extract tail of partial block. */
      nlanes = outlen / 8;
      nleft = (bsize - count) / 8;
      nlanes = nlanes < nleft ? nlanes : nleft;

      nburn = ctx->ops->extract(hd, count / 8, outbuf, nlanes * 8);
      burn = nburn > burn ? nburn : burn;
      outlen -= nlanes * 8;
      outbuf += nlanes * 8;
      count += nlanes * 8;

      gcry_assert(count <= bsize);

      if (count == bsize)
	count = 0;
    }

  while (outlen >= bsize)
    {
      gcry_assert(count == 0);

      /* Squeeze more. */
      nburn = ctx->ops->permute(hd);
      burn = nburn > burn ? nburn : burn;

      /* Extract full block. */
      nburn = ctx->ops->extract(hd, 0, outbuf, bsize);
      burn = nburn > burn ? nburn : burn;

      outlen -= bsize;
      outbuf += bsize;
    }

  if (outlen)
    {
      gcry_assert(outlen < bsize);

      if (count == 0)
	{
	  /* Squeeze more. */
	  nburn = ctx->ops->permute(hd);
	  burn = nburn > burn ? nburn : burn;
	}

      if (outlen >= 8)
	{
	  /* Extract head of partial block. */
	  nlanes = outlen / 8;
	  nburn = ctx->ops->extract(hd, count / 8, outbuf, nlanes * 8);
	  burn = nburn > burn ? nburn : burn;
	  outlen -= nlanes * 8;
	  outbuf += nlanes * 8;
	  count += nlanes * 8;

	  gcry_assert(count < bsize);
	}

      if (outlen)
	{
	  /* Extract head of partial lane. */
	  nburn = ctx->ops->extract(hd, count / 8, lane, 8);
	  burn = nburn > burn ? nburn : burn;

	  for (i = count % 8; outlen && i < 8; i++)
	    {
	      *outbuf++ = lane[i];
	      outlen--;
	      count++;
	    }

	  gcry_assert(count < bsize);
	}
    }

  ctx->count = count;

  if (burn)
    _gcry_burn_stack (burn);

  return 0;
}


static gcry_err_code_t
keccak_extract (void *context, void *out, size_t outlen)
{
  KECCAK_CONTEXT *ctx = context;

  if (ctx->shake_in_read_mode)
    return GPG_ERR_INV_STATE;
  if (!ctx->shake_in_extract_mode)
    ctx->shake_in_extract_mode = 1;

  return do_keccak_extract (context, out, outlen);
}


static byte *
keccak_shake_read (void *context)
{
  KECCAK_CONTEXT *ctx = (KECCAK_CONTEXT *) context;
  KECCAK_STATE *hd = &ctx->state;

  if (ctx->shake_in_extract_mode)
    {
      /* Already in extract mode. */
      return NULL;
    }

  if (!ctx->shake_in_read_mode)
    {
      byte tmpbuf[64];

      gcry_assert(sizeof(tmpbuf) >= ctx->outlen);

      ctx->shake_in_read_mode = 1;

      do_keccak_extract (context, tmpbuf, ctx->outlen);
      buf_cpy (&hd->u, tmpbuf, ctx->outlen);

      wipememory(tmpbuf, sizeof(tmpbuf));
    }

  return (byte *)&hd->u;
}


/* Variant of the above shortcut function using multiple buffers.  */
#define _gcry_sha3_hash_buffers 0

#define _gcry_sha3_224_hash_buffers 0
#define _gcry_sha3_256_hash_buffers 0
#define _gcry_sha3_384_hash_buffers 0
#define _gcry_sha3_512_hash_buffers 0
#define _gcry_shake128_hash_buffers 0
#define _gcry_shake256_hash_buffers 0

static unsigned int
cshake_input_n (KECCAK_CONTEXT *ctx, const void *n, unsigned int n_len)
{
  unsigned char buf[3];

  buf[0] = 1;
  buf[1] = ctx->blocksize;
  keccak_write (ctx, buf, 2);

  /* Here, N_LEN must be less than 255 */
  if (n_len < 32)
    {
      buf[0] = 1;
      buf[1] = n_len * 8;
    }
  else
    {
      buf[0] = 2;
      buf[1] = (n_len * 8) >> 8;
      buf[2] = (n_len * 8) & 0xff;
    }

  keccak_write (ctx, buf, buf[0] + 1);
  keccak_write (ctx, n, n_len);
  return 2 + buf[0] + 1 + n_len;
}

static void
cshake_input_s (KECCAK_CONTEXT *ctx, const void *s, unsigned int s_len,
                unsigned int len_written)
{
  unsigned char buf[168];
  unsigned int padlen;

  /* Here, S_LEN must be less than 255 */
  if (s_len < 32)
    {
      buf[0] = 1;
      buf[1] = s_len * 8;
    }
  else
    {
      buf[0] = 2;
      buf[1] = (s_len * 8) >> 8;
      buf[2] = (s_len * 8) & 0xff;
    }

  keccak_write (ctx, buf, buf[0] + 1);
  keccak_write (ctx, s, s_len);

  len_written += buf[0] + 1 + s_len;
  padlen = ctx->blocksize - (len_written % ctx->blocksize);
  memset (buf, 0, padlen);
  keccak_write (ctx, buf, padlen);
}

gpg_err_code_t
_gcry_cshake_customize (void *context, struct gcry_cshake_customization *p)
{
  KECCAK_CONTEXT *ctx = (KECCAK_CONTEXT *) context;
  unsigned int len_written;

  if (p->n_len >= 255 || p->s_len >= 255)
    return GPG_ERR_TOO_LARGE;

  if (p->n_len == 0 && p->s_len == 0)
    /* No customization */
    return 0;

  len_written = cshake_input_n (ctx, p->n, p->n_len);
  cshake_input_s (ctx, p->s, p->s_len, len_written);
  ctx->suffix = CSHAKE_DELIMITED_SUFFIX;
  return 0;
}


static void
cshake128_init (void *context, unsigned int flags)
{
  keccak_init (GCRY_MD_CSHAKE128, context, flags);
}

static void
cshake256_init (void *context, unsigned int flags)
{
  keccak_init (GCRY_MD_CSHAKE256, context, flags);
}


#define _gcry_cshake128_hash_buffers 0
#define _gcry_cshake256_hash_buffers 0
/*
     Self-test section.
 */




/* Run a full self-test for ALGO and return 0 on success.  */




/* Object IDs obtained from
 * https://csrc.nist.gov/projects/computer-security-objects-register/algorithm-registration#Hash
 */
static const byte sha3_224_asn[] =
  { 0x30, 0x2d, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48,
    0x01, 0x65, 0x03, 0x04, 0x02, 0x07, 0x05, 0x00, 0x04,
    0x1c
  };
static const gcry_md_oid_spec_t oid_spec_sha3_224[] =
  {
    { "2.16.840.1.101.3.4.2.7" },
    /* id-rsassa-pkcs1-v1-5-with-sha3-224 */
    { "2.16.840.1.101.3.4.3.13" },
    /* id-ecdsa-with-sha3-224 */
    { "2.16.840.1.101.3.4.3.9" },
    { NULL }
  };
static const byte sha3_256_asn[] =
  { 0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48,
    0x01, 0x65, 0x03, 0x04, 0x02, 0x08, 0x05, 0x00, 0x04,
    0x20
  };
static const gcry_md_oid_spec_t oid_spec_sha3_256[] =
  {
    { "2.16.840.1.101.3.4.2.8" },
    /* id-rsassa-pkcs1-v1-5-with-sha3-256 */
    { "2.16.840.1.101.3.4.3.14" },
    /* id-ecdsa-with-sha3-256 */
    { "2.16.840.1.101.3.4.3.10" },
    { NULL }
  };
static const byte sha3_384_asn[] =
  { 0x30, 0x41, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48,
    0x01, 0x65, 0x03, 0x04, 0x02, 0x09, 0x05, 0x00, 0x04,
    0x30
  };
static const gcry_md_oid_spec_t oid_spec_sha3_384[] =
  {
    { "2.16.840.1.101.3.4.2.9" },
    /* id-rsassa-pkcs1-v1-5-with-sha3-384 */
    { "2.16.840.1.101.3.4.3.15" },
    /* id-ecdsa-with-sha3-384 */
    { "2.16.840.1.101.3.4.3.11" },
    { NULL }
  };
static const byte sha3_512_asn[] =
  { 0x30, 0x51, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48,
    0x01, 0x65, 0x03, 0x04, 0x02, 0x0a, 0x05, 0x00, 0x04,
    0x40
  };
static const gcry_md_oid_spec_t oid_spec_sha3_512[] =
  {
    { "2.16.840.1.101.3.4.2.10" },
    /* id-rsassa-pkcs1-v1-5-with-sha3-512 */
    { "2.16.840.1.101.3.4.3.16" },
    /* id-ecdsa-with-sha3-512 */
    { "2.16.840.1.101.3.4.3.12" },
    { NULL }
  };
static const byte shake128_asn[] =
  { 0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48,
    0x01, 0x65, 0x03, 0x04, 0x02, 0x0b, 0x05, 0x00, 0x04,
    0x20
  };
static const gcry_md_oid_spec_t oid_spec_shake128[] =
  {
    { "2.16.840.1.101.3.4.2.11" },
    /* RFC 8692 id-RSASSA-PSS-SHAKE128 */
    { "1.3.6.1.5.5.7.6.30" },
    /* RFC 8692 id-ecdsa-with-shake128 */
    { "1.3.6.1.5.5.7.6.32" },
    { NULL }
  };
static const byte shake256_asn[] =
  { 0x30, 0x51, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48,
    0x01, 0x65, 0x03, 0x04, 0x02, 0x0c, 0x05, 0x00, 0x04,
    0x40
  };
static const gcry_md_oid_spec_t oid_spec_shake256[] =
  {
    { "2.16.840.1.101.3.4.2.12" },
    /* RFC 8692 id-RSASSA-PSS-SHAKE256 */
    { "1.3.6.1.5.5.7.6.31" },
    /* RFC 8692 id-ecdsa-with-shake256 */
    { "1.3.6.1.5.5.7.6.33" },
    { NULL }
  };

gcry_md_spec_t _gcry_digest_spec_sha3_224 =
  {
    GCRY_MD_SHA3_224, {0, 1},
    "SHA3-224", sha3_224_asn, DIM (sha3_224_asn), oid_spec_sha3_224, 28,
    sha3_224_init, keccak_write, keccak_final, keccak_read, NULL,
    _gcry_sha3_224_hash_buffers,
    sizeof (KECCAK_CONTEXT),
    GRUB_UTIL_MODNAME("gcry_keccak")
    .blocksize = 144.0
  };
gcry_md_spec_t _gcry_digest_spec_sha3_256 =
  {
    GCRY_MD_SHA3_256, {0, 1},
    "SHA3-256", sha3_256_asn, DIM (sha3_256_asn), oid_spec_sha3_256, 32,
    sha3_256_init, keccak_write, keccak_final, keccak_read, NULL,
    _gcry_sha3_256_hash_buffers,
    sizeof (KECCAK_CONTEXT),
    GRUB_UTIL_MODNAME("gcry_keccak")
    .blocksize = 136.0
  };
gcry_md_spec_t _gcry_digest_spec_sha3_384 =
  {
    GCRY_MD_SHA3_384, {0, 1},
    "SHA3-384", sha3_384_asn, DIM (sha3_384_asn), oid_spec_sha3_384, 48,
    sha3_384_init, keccak_write, keccak_final, keccak_read, NULL,
    _gcry_sha3_384_hash_buffers,
    sizeof (KECCAK_CONTEXT),
    GRUB_UTIL_MODNAME("gcry_keccak")
    .blocksize = 104.0
  };
gcry_md_spec_t _gcry_digest_spec_sha3_512 =
  {
    GCRY_MD_SHA3_512, {0, 1},
    "SHA3-512", sha3_512_asn, DIM (sha3_512_asn), oid_spec_sha3_512, 64,
    sha3_512_init, keccak_write, keccak_final, keccak_read, NULL,
    _gcry_sha3_512_hash_buffers,
    sizeof (KECCAK_CONTEXT),
    GRUB_UTIL_MODNAME("gcry_keccak")
    .blocksize = 72.0
  };
gcry_md_spec_t _gcry_digest_spec_shake128 =
  {
    GCRY_MD_SHAKE128, {0, 1},
    "SHAKE128", shake128_asn, DIM (shake128_asn), oid_spec_shake128, 32,
    shake128_init, keccak_write, keccak_final, keccak_shake_read,
    keccak_extract,
    _gcry_shake128_hash_buffers,
    sizeof (KECCAK_CONTEXT),
    GRUB_UTIL_MODNAME("gcry_keccak")
    .blocksize = 64
  };
gcry_md_spec_t _gcry_digest_spec_shake256 =
  {
    GCRY_MD_SHAKE256, {0, 1},
    "SHAKE256", shake256_asn, DIM (shake256_asn), oid_spec_shake256, 64,
    shake256_init, keccak_write, keccak_final, keccak_shake_read,
    keccak_extract,
    _gcry_shake256_hash_buffers,
    sizeof (KECCAK_CONTEXT),
    GRUB_UTIL_MODNAME("gcry_keccak")
    .blocksize = 64
  };
gcry_md_spec_t _gcry_digest_spec_cshake128 =
  {
    GCRY_MD_CSHAKE128, {0, 1},
    "CSHAKE128", NULL, 0, NULL, 32,
    cshake128_init, keccak_write, keccak_final, keccak_shake_read,
    keccak_extract, _gcry_cshake128_hash_buffers,
    sizeof (KECCAK_CONTEXT),
    GRUB_UTIL_MODNAME("gcry_keccak")
    .blocksize = 64
  };
gcry_md_spec_t _gcry_digest_spec_cshake256 =
  {
    GCRY_MD_CSHAKE256, {0, 1},
    "CSHAKE256", NULL, 0, NULL, 64,
    cshake256_init, keccak_write, keccak_final, keccak_shake_read,
    keccak_extract, _gcry_cshake256_hash_buffers,
    sizeof (KECCAK_CONTEXT),
    GRUB_UTIL_MODNAME("gcry_keccak")
    .blocksize = 64
  };


GRUB_MOD_INIT(gcry_keccak)
{
  grub_md_register (&_gcry_digest_spec_sha3_224);
  grub_md_register (&_gcry_digest_spec_sha3_256);
  grub_md_register (&_gcry_digest_spec_sha3_384);
  grub_md_register (&_gcry_digest_spec_sha3_512);
  grub_md_register (&_gcry_digest_spec_shake128);
  grub_md_register (&_gcry_digest_spec_shake256);
  grub_md_register (&_gcry_digest_spec_cshake128);
  grub_md_register (&_gcry_digest_spec_cshake256);
}

GRUB_MOD_FINI(gcry_keccak)
{
  grub_md_unregister (&_gcry_digest_spec_sha3_224);
  grub_md_unregister (&_gcry_digest_spec_sha3_256);
  grub_md_unregister (&_gcry_digest_spec_sha3_384);
  grub_md_unregister (&_gcry_digest_spec_sha3_512);
  grub_md_unregister (&_gcry_digest_spec_shake128);
  grub_md_unregister (&_gcry_digest_spec_shake256);
  grub_md_unregister (&_gcry_digest_spec_cshake128);
  grub_md_unregister (&_gcry_digest_spec_cshake256);
}
