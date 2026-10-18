#pragma once

#include <cmath>
#include <cstdio>

inline int gChecks = 0;
inline int gFailures = 0;

#define CHECK(cond) do { ++gChecks; if (!(cond)) { ++gFailures; \
    std::printf ("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

#define CHECK_NEAR(a, b, tol) do { ++gChecks; \
    const double _a = (a), _b = (b); \
    if (std::fabs (_a - _b) > (tol)) { ++gFailures; \
        std::printf ("FAIL %s:%d  %s = %.9g, expected %.9g\n", \
                     __FILE__, __LINE__, #a, _a, _b); } } while (0)

inline int reportAndExit()
{
    std::printf ("%d checks, %d failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
