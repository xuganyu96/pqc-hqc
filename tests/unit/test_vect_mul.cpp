#include <NTL/GF2X.h>
#include <cstdint>
#include <cstring>

#ifdef HQC_X86_IMPL
#include <immintrin.h>
#endif

extern "C" {
#include "gf2x.h"
#include "munit.h"
#include "parameters.h"
}

static void mask_last_word(uint64_t *v) {
    v[VEC_N_SIZE_64 - 1] &= BITMASK(PARAM_N, 64);
}

static void words_to_poly(NTL::GF2X &out, const uint64_t *words) {
    clear(out);
    for (size_t bit = 0; bit < PARAM_N; ++bit) {
        size_t word = bit / 64;
        size_t bitpos = bit % 64;
        if ((words[word] >> bitpos) & 1ULL) {
            SetCoeff(out, static_cast<long>(bit));
        }
    }
}

static void poly_to_words(uint64_t *out, const NTL::GF2X &poly) {
    std::memset(out, 0, VEC_N_SIZE_64 * sizeof(uint64_t));
    for (size_t bit = 0; bit < PARAM_N; ++bit) {
        if (NTL::IsOne(NTL::coeff(poly, static_cast<long>(bit)))) {
            size_t word = bit / 64;
            size_t bitpos = bit % 64;
            out[word] |= (1ULL << bitpos);
        }
    }
    mask_last_word(out);
}

static void set_bit(uint64_t *v, size_t bit) {
    if (bit >= PARAM_N) {
        return;
    }
    size_t word = bit / 64;
    size_t bitpos = bit % 64;
    v[word] |= (1ULL << bitpos);
}

#ifdef HQC_X86_IMPL
#define VEC_N_256_SIZE_VEC (VEC_N_256_SIZE_64 / 4)

static void pack_to_avx2(__m256i *out, const uint64_t *in) {
    uint64_t *out64 = reinterpret_cast<uint64_t *>(out);
    std::memset(out64, 0, VEC_N_256_SIZE_64 * sizeof(uint64_t));
    std::memcpy(out64, in, VEC_N_SIZE_64 * sizeof(uint64_t));
}

static void unpack_from_avx2(uint64_t *out, const __m256i *in) {
    const uint64_t *in64 = reinterpret_cast<const uint64_t *>(in);
    std::memcpy(out, in64, VEC_N_SIZE_64 * sizeof(uint64_t));
    mask_last_word(out);
}
#endif

static int ntl_check_vect_mul(const uint64_t *a, const uint64_t *b) {
    uint64_t got[VEC_N_SIZE_64] = {0};
    uint64_t expected[VEC_N_SIZE_64] = {0};

#ifdef HQC_X86_IMPL
    __m256i a256[VEC_N_256_SIZE_VEC];
    __m256i b256[VEC_N_256_SIZE_VEC];
    __m256i o256[VEC_N_256_SIZE_VEC];

    pack_to_avx2(a256, a);
    pack_to_avx2(b256, b);
    vect_mul(o256, a256, b256);
    unpack_from_avx2(got, o256);
#else
    vect_mul(got, a, b);
#endif

    NTL::GF2X A, B, C;
    words_to_poly(A, a);
    words_to_poly(B, b);

    NTL::GF2X f;
    SetCoeff(f, PARAM_N);
    SetCoeff(f, 0);
    NTL::MulMod(C, A, B, f);
    poly_to_words(expected, C);

    return std::memcmp(got, expected, sizeof(got)) == 0 ? 0 : 1;
}

static MunitResult test_vect_mul_fixed(const MunitParameter params[], void *user_data) {
    (void)params;
    (void)user_data;

    uint64_t a[VEC_N_SIZE_64] = {0};
    uint64_t b[VEC_N_SIZE_64] = {0};

    munit_assert_int(ntl_check_vect_mul(a, b), ==, 0);

    a[0] = 1;
    b[0] = 1;
    munit_assert_int(ntl_check_vect_mul(a, b), ==, 0);

    std::memset(a, 0, sizeof(a));
    std::memset(b, 0, sizeof(b));
    set_bit(a, 0);
    set_bit(b, PARAM_N - 1);
    munit_assert_int(ntl_check_vect_mul(a, b), ==, 0);

    std::memset(a, 0, sizeof(a));
    std::memset(b, 0, sizeof(b));
    set_bit(a, PARAM_N - 1);
    set_bit(b, 1);
    munit_assert_int(ntl_check_vect_mul(a, b), ==, 0);

    std::memset(a, 0, sizeof(a));
    std::memset(b, 0, sizeof(b));
    set_bit(a, PARAM_N - 1);
    set_bit(b, 0);
    munit_assert_int(ntl_check_vect_mul(a, b), ==, 0);

    std::memset(a, 0, sizeof(a));
    std::memset(b, 0, sizeof(b));
    set_bit(a, PARAM_N - 1);
    set_bit(b, PARAM_N - 1);
    munit_assert_int(ntl_check_vect_mul(a, b), ==, 0);

    std::memset(a, 0, sizeof(a));
    std::memset(b, 0, sizeof(b));
    set_bit(a, PARAM_N / 2);
    set_bit(b, PARAM_N / 2);
    munit_assert_int(ntl_check_vect_mul(a, b), ==, 0);

    std::memset(a, 0, sizeof(a));
    std::memset(b, 0, sizeof(b));
    set_bit(a, 0);
    set_bit(a, PARAM_N - 1);
    set_bit(b, 1);
    set_bit(b, PARAM_N - 2);
    munit_assert_int(ntl_check_vect_mul(a, b), ==, 0);

    std::memset(a, 0xFF, sizeof(a));
    std::memset(b, 0xFF, sizeof(b));
    mask_last_word(a);
    mask_last_word(b);
    munit_assert_int(ntl_check_vect_mul(a, b), ==, 0);

    return MUNIT_OK;
}

static MunitResult test_vect_mul_random(const MunitParameter params[], void *user_data) {
    (void)params;
    (void)user_data;

    uint64_t a[VEC_N_SIZE_64] = {0};
    uint64_t b[VEC_N_SIZE_64] = {0};

    for (int i = 0; i < 100; ++i) {
        munit_rand_memory(sizeof(a), reinterpret_cast<munit_uint8_t *>(a));
        munit_rand_memory(sizeof(b), reinterpret_cast<munit_uint8_t *>(b));
        mask_last_word(a);
        mask_last_word(b);
        munit_assert_int(ntl_check_vect_mul(a, b), ==, 0);
    }

    return MUNIT_OK;
}

extern "C" {
MunitTest gf2x_ntl_tests[] = {
    {(char *)"/vect_mul/fixed", test_vect_mul_fixed, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char *)"/vect_mul/random", test_vect_mul_random, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};
}
