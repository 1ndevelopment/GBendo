/* Unity shim: prefer upstream Unity if available; otherwise fall back to minimal harness */
#ifndef VENDOR_UNITY_H
#define VENDOR_UNITY_H

#if defined(__has_include)
#  if __has_include("../unity/unity.h")
#    include "../unity/unity.h"
     /* Compatibility: allow UnityBegin() without args */
#    ifndef UnityBegin
#      define UnityBegin() UnityBegin((const char*)0)
#    endif
#  else
#    define GBENDO_USE_MINI_UNITY 1
#  endif
#else
#  define GBENDO_USE_MINI_UNITY 1
#endif

#if defined(GBENDO_USE_MINI_UNITY)
#include <stdio.h>
#include <stdlib.h>

static int UnityTestsRun = 0;
static int UnityTestsFailed = 0;
static int UnityCurrentTestFailed = 0;

static void UnityBegin(void) {
    UnityTestsRun = 0;
    UnityTestsFailed = 0;
}

static int UnityEnd(void) {
    if (UnityTestsFailed == 0) {
        printf("ALL TESTS PASSED (%d)\n", UnityTestsRun);
    } else {
        printf("TESTS FAILED: %d/%d\n", UnityTestsFailed, UnityTestsRun);
    }
    return UnityTestsFailed != 0;
}

/* Run a single test function (void func(void)) */
#define RUN_TEST(func) do { \
    UnityCurrentTestFailed = 0; \
    printf("RUN: %s\n", #func); \
    func(); \
    UnityTestsRun++; \
    if (UnityCurrentTestFailed) { \
        UnityTestsFailed++; \
        printf("FAIL: %s\n", #func); \
    } else { \
        printf("PASS: %s\n", #func); \
    } \
} while(0)

/* Assertions */
#define TEST_ASSERT_TRUE_MESSAGE(expr, msg) do { \
    if (!(expr)) { \
        printf("  Assertion failed: %s (message: %s)\n", #expr, (msg)); \
        UnityCurrentTestFailed = 1; \
        return; \
    } \
} while(0)

#define TEST_ASSERT_FALSE_MESSAGE(expr, msg) do { \
    if (expr) { \
        printf("  Assertion failed (expected false): %s (message: %s)\n", #expr, (msg)); \
        UnityCurrentTestFailed = 1; \
        return; \
    } \
} while(0)

#define TEST_ASSERT_EQUAL_INT(expected, actual) do { \
    int _e = (expected); int _a = (actual); \
    if (_e != _a) { \
        printf("  Assertion failed: expected %d, got %d\n", _e, _a); \
        UnityCurrentTestFailed = 1; \
        return; \
    } \
} while(0)

#define TEST_ASSERT_EQUAL_UINT8(expected, actual) do { \
    unsigned int _e = (unsigned int)(expected); unsigned int _a = (unsigned int)(actual); \
    if (_e != _a) { \
        printf("  Assertion failed: expected 0x%02X, got 0x%02X\n", _e, _a); \
        UnityCurrentTestFailed = 1; \
        return; \
    } \
} while(0)
#endif /* GBENDO_USE_MINI_UNITY */

#endif /* VENDOR_UNITY_H */
