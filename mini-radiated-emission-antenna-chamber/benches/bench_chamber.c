/* =========================================================================
 * bench_chamber.c - Performance benchmarks for chamber computations
 * ========================================================================= */
#include "radiated_emission.h"
#include "antenna_types.h"
#include "chamber_theory.h"
#include "field_propagation.h"
#include "emission_limits.h"
#include <stdio.h>
#include <time.h>

#define ITERATIONS 10000

static double bench_af_compute(void) {
    double sum = 0.0;
    for (int i = 0; i < ITERATIONS; i++) {
        sum += re_antenna_factor_from_gain(100e6 + i*100e3, 1.64);
    }
    return sum;
}
static double bench_field_strength(void) {
    double sum = 0.0;
    for (int i = 0; i < ITERATIONS; i++) {
        sum += re_field_strength_compute(-80.0 + i*0.001, 10.0, 3.0, 20.0, 50.0);
    }
    return sum;
}
static double bench_nsa(void) {
    double sum = 0.0;
    for (int i = 0; i < ITERATIONS; i++) {
        sum += ct_nsa_theoretical(3.0, 2.0, 1.5, 300e6 + i*10e3, RE_POL_HORIZONTAL);
    }
    return sum;
}
static double bench_limit_query(void) {
    double sum = 0.0;
    for (int i = 0; i < ITERATIONS; i++) {
        sum += el_get_standard_limit(RE_STD_CISPR32, 100e6 + i*100e3,
                                      RE_DETECTOR_QUASI_PEAK, 3.0, 1);
    }
    return sum;
}
int main(void) {
    printf("=== Radiated Emission Benchmarks ===
");
    printf("Iterations per test: %d

", ITERATIONS);
    clock_t start, end;
    double result;
    start = clock(); result = bench_af_compute(); end = clock();
    printf("Antenna Factor: %.3f ms (sum=%.1f)
", 1000.0*(end-start)/CLOCKS_PER_SEC, result);
    start = clock(); result = bench_field_strength(); end = clock();
    printf("Field Strength: %.3f ms (sum=%.1f)
", 1000.0*(end-start)/CLOCKS_PER_SEC, result);
    start = clock(); result = bench_nsa(); end = clock();
    printf("NSA Theoretical: %.3f ms (sum=%.1f)
", 1000.0*(end-start)/CLOCKS_PER_SEC, result);
    start = clock(); result = bench_limit_query(); end = clock();
    printf("Limit Query: %.3f ms (sum=%.1f)
", 1000.0*(end-start)/CLOCKS_PER_SEC, result);
    return 0;
}
