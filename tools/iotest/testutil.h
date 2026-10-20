/* Minimal test helpers: counted checks and a final report. */
#ifndef IOTEST_TESTUTIL_H
#define IOTEST_TESTUTIL_H

#include <stdio.h>

static int test_failures;
static int test_checks;

#define CHECK(cond)                                                                          \
    do {                                                                                     \
        test_checks++;                                                                       \
        if (!(cond)) {                                                                       \
            test_failures++;                                                                 \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                  \
        }                                                                                    \
    } while (0)

#define CHECK_EQ_INT(a, b)                                                                   \
    do {                                                                                     \
        long a_ = (long)(a), b_ = (long)(b);                                                 \
        test_checks++;                                                                       \
        if (a_ != b_) {                                                                      \
            test_failures++;                                                                 \
            fprintf(stderr, "FAIL %s:%d: %s == %s (%ld vs %ld)\n", __FILE__, __LINE__, #a,   \
                    #b, a_, b_);                                                             \
        }                                                                                    \
    } while (0)

static int test_report(const char *name)
{
    printf("%s: %d checks, %d failures\n", name, test_checks, test_failures);
    return test_failures ? 1 : 0;
}

#endif
