#ifndef MINUNIT_H
#define MINUNIT_H

#include <stdio.h>
#include <stdlib.h>

#define mu_assert(message, test) do { if (!(test)) { printf("FAIL: %s\n", message); return 1; } } while (0)
#define mu_run_test(test) do { printf("RUN: %s\n", #test); int rc = test(); if (rc != 0) return rc; } while(0)

#endif /* MINUNIT_H */
