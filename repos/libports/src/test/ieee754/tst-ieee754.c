/* Some IEEE-754 / ISO C99+ conformance tests.
 *
 * Compile this program with:
 *   gcc -Wall -O2 -std=c99 tst-ieee754.c -o tst-ieee754 -lm
 * for instance.
 *
 * Add -DFP_CONTRACT to allow contraction of FP expressions (e.g. with icc).
 *
 * Copyright 2003-2017 Vincent Lefevre <vincent@vinc17.net>.
 *
 * You may use this software under the terms of the MIT License:
 *   http://opensource.org/licenses/MIT
 *
 * More information: https://en.wikipedia.org/wiki/MIT_License
 */

#define SVNID "$Id: tst-ieee754.c 98609 2017-05-19 18:35:55Z vinc17/zira $"

#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <float.h>
#include <math.h>

#define STRINGIFY(S) #S
#define MAKE_STR(S) STRINGIFY(S)

#ifdef FP_CONTRACT
#undef FP_CONTRACT
#define FP_CONTRACT "ON"
#pragma STDC FP_CONTRACT ON
#else
#define FP_CONTRACT "OFF"
#pragma STDC FP_CONTRACT OFF
#endif

#ifndef NO_FENV_H
#include <fenv.h>
#pragma STDC FENV_ACCESS ON
#endif

#ifndef NAN
#define NAN (0.0/0.0)
#endif

#ifndef INFINITY
#define INFINITY (1.0/0.0)
#endif

#define DBL_NAN (NAN)
#define DBL_POS_INF (INFINITY)
#define DBL_NEG_INF (- DBL_POS_INF)

/* Note: The dynamic epsilon now gives information about
   - the possible extended precision used to evaluate expression;
   - the possible reduced precision due to the use of options like
     GCC's -mpc32 or -mpc64 to reduce the dynamic rounding precision
     with "387" arithmetic on x86 processors. */
#define PREC_EPSILON(T,V,F)                                             \
  do                                                                    \
    {                                                                   \
      volatile T eps = 1.0;                                             \
      printf (#V " = %" F "g = %" F "a\n", (T) (V), (T) (V));           \
      while (eps != 0)                                                  \
        {                                                               \
          volatile T x = 1.0, e = eps / FLT_RADIX;                      \
          x = (x + e) - 1.0;                                            \
          if (x != e)                                                   \
            break;                                                      \
          eps = e;                                                      \
        }                                                               \
      if (eps == 0)                                                     \
        printf ("  (cannot compute the dynamic epsilon)\n");            \
      else if (eps != (V))                                              \
        printf ("  (dynamic epsilon = %" F "g = %" F "a)\n", eps, eps); \
    }                                                                   \
  while (0)

#define ERRSTR(X) ((X) ? " [ERROR]" : "")

static float flt_max = FLT_MAX;
static double dbl_max = DBL_MAX;
static long double ldbl_max = LDBL_MAX;

static float flt_epsilon = FLT_EPSILON;
static double dbl_epsilon = DBL_EPSILON;
static long double ldbl_epsilon = LDBL_EPSILON;

/* <float.h> constants */
static void float_h (void)
{
  printf ("FLT_RADIX = %d\n", (int) FLT_RADIX);
  printf ("FLT_MANT_DIG = %d\n", (int) FLT_MANT_DIG);
  printf ("DBL_MANT_DIG = %d\n", (int) DBL_MANT_DIG);
  printf ("LDBL_MANT_DIG = %d\n\n", (int) LDBL_MANT_DIG);

  printf ("FLT_MIN_EXP = %d\n", (int) FLT_MIN_EXP);
  printf ("DBL_MIN_EXP = %d\n", (int) DBL_MIN_EXP);
  printf ("LDBL_MIN_EXP = %d\n\n", (int) LDBL_MIN_EXP);

  printf ("FLT_MAX_EXP = %d\n", (int) FLT_MAX_EXP);
  printf ("DBL_MAX_EXP = %d\n", (int) DBL_MAX_EXP);
  printf ("LDBL_MAX_EXP = %d\n\n", (int) LDBL_MAX_EXP);

  PREC_EPSILON (float, FLT_EPSILON, "");
  PREC_EPSILON (double, DBL_EPSILON, "");
  PREC_EPSILON (long double, LDBL_EPSILON, "L");
  putchar ('\n');

  printf ("FLT_MIN = %g = %a\n", (double) FLT_MIN, (double) FLT_MIN);
  printf ("DBL_MIN = %g = %a\n", (double) DBL_MIN, (double) DBL_MIN);
  printf ("LDBL_MIN = %Lg = %La\n\n",
          (long double) LDBL_MIN, (long double) LDBL_MIN);

  printf ("FLT_MAX = %g = %a\n", (double) FLT_MAX, (double) FLT_MAX);
  printf ("DBL_MAX = %g = %a\n", (double) DBL_MAX, (double) DBL_MAX);
  printf ("LDBL_MAX = %Lg = %La\n\n",
          (long double) LDBL_MAX, (long double) LDBL_MAX);
}

#define TSIZEOF(T) printf ("sizeof(" #T ") = %d\n", (int) sizeof(T))

static void float_sizeof (void)
{
  TSIZEOF (float);
  TSIZEOF (double);
  TSIZEOF (long double);
  putchar ('\n');
}

static void tstcast (void)
{
  double x;
  x = (double) 0;
  printf ("(double) 0 = %g\n", x);
}

/* This mostly tests signed zero support. This test is written in
   such a way that "gcc -O -ffast-math" gives a wrong result. */
static void signed_zero_inf (void)
{
  double x = 0.0, y = -0.0, px, py, nx, ny;

  printf ("Signed zero tests (x is 0.0 and y is -0.0):\n");

  if (x == y)
    printf ("  Test 1.0 / x != 1.0 / y  returns %d (should be 1).\n",
            1.0 / x != 1.0 / y);
  else
    printf ("x != y; this is wrong!\n");

  px = +x;
  if (x == px)
    printf ("  Test 1.0 / x == 1.0 / +x returns %d (should be 1).\n",
            1.0 / x == 1.0 / px);
  else
    printf ("x != +x; this is wrong!\n");

  py = +y;
  if (x == py)
    printf ("  Test 1.0 / x != 1.0 / +y returns %d (should be 1).\n",
            1.0 / x != 1.0 / py);
  else
    printf ("x != +y; this is wrong!\n");

  nx = -x;
  if (x == nx)
    printf ("  Test 1.0 / x != 1.0 / -x returns %d (should be 1).\n",
            1.0 / x != 1.0 / nx);
  else
    printf ("x != -x; this is wrong!\n");

  ny = -y;
  if (x == ny)
    printf ("  Test 1.0 / x == 1.0 / -y returns %d (should be 1).\n",
            1.0 / x == 1.0 / ny);
  else
    printf ("x != -y; this is wrong!\n");
}

static void tstadd (double x, double y)
{
  double a, s;

  a = x + y;
  s = x - y;
  printf ("%g + %g = %g\n", x, y, a);
  printf ("%g - %g = %g\n", x, y, s);
}

static void tstmul (double x, double y)
{
  double m;

  m = x * y;
  printf ("%g * %g = %g\n", x, y, m);
}

#define TSTCONST(S,OP)                                          \
  printf ("Constant expression 1 " S " DBL_MIN = %.20g\n"       \
          "Variable expression 1 " S " DBL_MIN = %.20g\n",      \
          1.0 OP DBL_MIN, 1.0 OP x);

static void tstconst (void)
{
  volatile double x = DBL_MIN;

  TSTCONST ("+", +);
  TSTCONST ("-", -);
}

#define TSTDIV(T,S)                                     \
  do                                                    \
    {                                                   \
      volatile T x = 1.0, y = 3.0;                      \
      x /= y;                                           \
      printf ("1/3 in %-12s: %" S "a\n", #T, x);        \
    }                                                   \
  while (0)

static void tstpow (void)
{
  double val[] = { 0.0, 0.0, 0.0, +0.0, -0.0,
                   +0.5, -0.5, +1.0, -1.0, +2.0, -2.0 };
  int i, j;

  /* Not used above to avoid an error with IRIX64 cc. */
  val[0] = DBL_NAN;
  val[1] = DBL_POS_INF;
  val[2] = DBL_NEG_INF;

  for (i = 0; i < sizeof (val) / sizeof (val[0]); i++)
    for (j = 0; j < sizeof (val) / sizeof (val[0]); j++)
      {
        double p;
        p = pow (val[i], val[j]);
        printf ("pow(%g, %g) = %g\n", val[i], val[j], p);
      }
}

static void tstall (void)
{
  float fm = FLT_MAX, fe = FLT_EPSILON;
  double dm = DBL_MAX, de = DBL_EPSILON;
  long double lm = LDBL_MAX, le = LDBL_EPSILON;

  tstcast ();
  signed_zero_inf ();

  tstadd (+0.0, +0.0);
  tstadd (+0.0, -0.0);
  tstadd (-0.0, +0.0);
  tstadd (-0.0, -0.0);
  tstadd (+1.0, +1.0);
  tstadd (+1.0, -1.0);

  tstmul (+0.0, +0.0);
  tstmul (+0.0, -0.0);
  tstmul (-0.0, +0.0);
  tstmul (-0.0, -0.0);

  tstconst ();
  TSTDIV (float, "");
  TSTDIV (double, "");
  TSTDIV (long double, "L");

  printf ("Dec 1.1  = %a\n", (double) 1.1);
  printf ("FLT_MAX  = %a%s\n", (double) fm, ERRSTR (fm != flt_max));
  printf ("DBL_MAX  = %a%s\n", dm, ERRSTR (dm != dbl_max));
  printf ("LDBL_MAX = %La%s\n", lm, ERRSTR (lm != ldbl_max));
  printf ("FLT_EPSILON  = %a%s\n", (double) fe, ERRSTR (fe != flt_epsilon));
  printf ("DBL_EPSILON  = %a%s\n", de, ERRSTR (de != dbl_epsilon));
  printf ("LDBL_EPSILON = %La%s\n", le, ERRSTR (le != ldbl_epsilon));

  tstpow ();
}

static void tsteval_method (void)
{
  volatile double x, y, z;

#if __STDC__ == 1 && __STDC_VERSION__ >= 199901 && defined(__STDC_IEC_559__)
  printf ("__STDC_IEC_559__ defined:\n"
          "The implementation shall conform to the IEEE-754 standard.\n");
# ifdef FLT_EVAL_METHOD
  printf ("FLT_EVAL_METHOD is %d (see ISO/IEC 9899, 5.2.4.2.2#8).\n\n",
          (int) FLT_EVAL_METHOD);
# else
  printf ("But FLT_EVAL_METHOD is not defined!\n\n");
# endif
#endif

  x = 9007199254740994.0; /* 2^53 + 2 */
  y = 1.0 - 1/65536.0;
  z = x + y;
  printf ("x + y, with x = 9007199254740994.0 and y = 1.0 - 1/65536.0"
          " (type double).\n"
          "The IEEE-754 result is 9007199254740994 with double precision.\n"
          "The IEEE-754 result is 9007199254740996 with extended precision.\n"
          "The obtained result is %.17g.\n", z);

  if (z == 9007199254740996.0)  /* computations in extended precision */
    {
      volatile double a, b;
      double c;

      a = 9007199254740992.0; /* 2^53 */
      b = a + 0.25;
      c = a + 0.25;
      if (b != c)
        printf ("\nBUG:\nThe implementation doesn't seem to convert values "
                "to the target type after\nan assignment (see ISO/IEC 9899: "
                "5.1.2.3#12, 6.3.1.5#2 and 6.3.1.8#2[52]).\n");
    }
}

/* This test is useful only on implementations where the "double" type
 * corresponds to the IEEE-754 double precision and the "long double"
 * type corresponds to the traditional x86 extended precision, but let's
 * do it in any case. It shows a bug in gcc 3.4 to 4.3.3 on x86_64 and
 * ia64 platforms. See:
 *     https://gcc.gnu.org/bugzilla/show_bug.cgi?id=36578
 */
static void ldcast_test (void)
{
  volatile double a = 4294967219.0;
  volatile double b = 4294967429.0;
  double c, d;
  long double al, bl;

  al = a;
  bl = b;
  c = (long double) a * (long double) b;
  d = al * bl;
  if (c != d)
    printf ("\nBUG: Casts to long double do not seem to be taken into "
            "account when\nthe result to stored to a variable of type "
            "double. If your compiler\nis gcc (version < 4.3.4), this "
            "may be the following bug:\n    "
            "https://gcc.gnu.org/bugzilla/show_bug.cgi?id=36578\n");
}

static void tstnan (void)
{
  double d;

  /* Various tests to detect a NaN without using the math library (-lm).
   * MIPSpro 7.3.1.3m (IRIX64) does too many optimisations, so that
   * both NAN != NAN and !(NAN >= 0.0 || NAN <= 0.0) give 0 instead
   * of 1. As a consequence, in MPFR, one needs to use
   *    #define DOUBLE_ISNAN(x) (!(((x) >= 0.0) + ((x) <= 0.0)))
   * in this case. */

  d = NAN;
  printf ("\n");
  printf ("NAN != NAN --> %d (should be 1)\n", d != d);
  printf ("isnan(NAN) --> %d (should be 1)\n", isnan (d));
  printf ("NAN >= 0.0 --> %d (should be 0)\n", d >= 0.0);
  printf ("NAN <= 0.0 --> %d (should be 0)\n", d <= 0.0);
  printf ("  #3||#4   --> %d (should be 0)\n", d >= 0.0 || d <= 0.0);
  printf ("!(#3||#4)  --> %d (should be 1)\n", !(d >= 0.0 || d <= 0.0));
  printf ("  #3 + #4  --> %d (should be 0)\n", (d >= 0.0) + (d <= 0.0));
  printf ("!(#3 + #4) --> %d (should be 1)\n", !((d >= 0.0) + (d <= 0.0)));
}

#define TSTINVALID(F,C)                                                 \
  do                                                                    \
    {                                                                   \
      feclearexcept (FE_INVALID);                                       \
      (void) (d C 0.0);                                                 \
      if ((F) ^ ! fetestexcept (FE_INVALID))                            \
        printf ("The FE_INVALID flag is%s set for NAN " #C " 0.\n",     \
                (F) ? "" : " not");                                     \
    }                                                                   \
  while (0)

static void tstinvalid (void)
{
#ifdef NO_FENV_H
  printf ("The FE_INVALID flag could not be tested (no <fenv.h>)\n");
#else
  double d = NAN;
  TSTINVALID(1,==);
  TSTINVALID(1,!=);
  TSTINVALID(0,>=);
  TSTINVALID(0,<=);
  TSTINVALID(0,>);
  TSTINVALID(0,<);
#endif
}

/* Note: we do not use the FP_CONTRACT pragma locally (in a block) as
   icc 10.1 seems to disable contraction when it sees FP_CONTRACT OFF
   somewhere in the source. */
static void fused_madd_test (void)
{
#define TWO40 (1099511627776.0)  /* 2^40 */
#define C1U40 (1.0 + 1.0/TWO40)  /* 1 + 2^(-40) */
  volatile double x = C1U40, y = C1U40, z = -1.0, d;

  d = x * y + z;
  printf ("\nx * y + z with FP_CONTRACT " FP_CONTRACT " is %sfused.\n",
          d == 2.0 * (1 + 0.5 / TWO40) / TWO40 ? "" : "not ");
}

/* FE_INVALID exception with Clang:
 *   https://bugs.llvm.org//show_bug.cgi?id=17686
 */
static void double_to_unsigned (void)
{
  volatile double d;
  uint64_t i = (uint64_t) 1 << 63;
  int t1, t2;

  d = i;
  feclearexcept (FE_INVALID);
  t1 = (uint64_t) d != i;
  t2 = fetestexcept (FE_INVALID);
  if (t1 || t2)
    printf ("\nError in cast of double to unsigned: %s value%s\n",
            t1 ? "incorrect" : "correct", t2 ? ", FE_INVALID" : "");
}

static void ibm_ldconv (void)
{
#define CAT1(X) 1 ## X
#define CAT2(X) CAT1(X)
#define LD0 .000000000000000000000000000000000001L
#define LD1 CAT2(LD0)
  long double x = 1.0L + LD0, y = LD1;

  if (x > 1.0L && y == 1.0L)
    {
      printf ("\nBad conversion of " MAKE_STR(LD1) "\n");
      printf ("Got 1 instead of about 1 + %La\n", x - 1.0L);
    }
}

int main (void)
{
  /* printf ("%s\n\n", SVNID); */

  float_h ();
  float_sizeof ();
  tsteval_method ();
  ldcast_test ();
  tstnan ();
  tstinvalid ();
  fused_madd_test ();
  double_to_unsigned ();

  if (LDBL_MIN_EXP == -968 && LDBL_MAX_EXP == 1024 &&
      LDBL_MANT_DIG == 106)  /* IBM long double format, i.e. double-double */
    ibm_ldconv ();

  printf ("\nRounding to nearest\n");
#ifdef FE_TONEAREST
  if (fesetround (FE_TONEAREST))
    printf ("Error, but let's do the test since it "
            "should be the default rounding mode.\n");
#endif
  tstall ();

#ifdef FE_TOWARDZERO
  printf ("\nRounding toward 0\n");
  if (fesetround (FE_TOWARDZERO))
    printf ("Error\n");
  else
    tstall ();
#endif

#ifdef FE_DOWNWARD
  printf ("\nRounding to -oo\n");
  if (fesetround (FE_DOWNWARD))
    printf ("Error\n");
  else
    tstall ();
#endif

#ifdef FE_UPWARD
  printf ("\nRounding to +oo\n");
  if (fesetround (FE_UPWARD))
    printf ("Error\n");
  else
    tstall ();
#endif

  return 0;
}
