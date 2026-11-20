#ifndef HEART_RATE_NUMERIC_PROFILE_H
#define HEART_RATE_NUMERIC_PROFILE_H
/* V27 preserves the V6 method and gates; this selects only the numerical
 * spectrum implementation. Numerical equivalence is tested, not assumed. */
#ifndef HR_NUMERIC_BACKEND
#define HR_NUMERIC_BACKEND 1
#endif
#if HR_NUMERIC_BACKEND == 0
#define HR_NUMERIC_NAME "SHARED_F64_REFERENCE"
#elif HR_NUMERIC_BACKEND == 1
#define HR_NUMERIC_NAME "CHIP_F32_DIRECT"
#elif HR_NUMERIC_BACKEND == 2
#define HR_NUMERIC_NAME "CHIP_F32_RECURRENCE"
#elif HR_NUMERIC_BACKEND == 3
#define HR_NUMERIC_NAME "CHIP_F32_DIRECT_COMPENSATED"
#else
#error "Unsupported heart numeric backend"
#endif
#endif
