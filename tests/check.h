#ifndef KAGEMUSHA_CHECK_H
#define KAGEMUSHA_CHECK_H

#include <stdio.h>

static int failures;
static int checks;

#define CHECK(cond)                                                                       \
	do {                                                                              \
		checks++;                                                                 \
		if (!(cond)) {                                                            \
			failures++;                                                       \
			printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);            \
		}                                                                         \
	} while (0)

#define CHECK_DONE()                                                                      \
	do {                                                                              \
		printf("%d checks, %d failed\n", checks, failures);                       \
		return failures ? 1 : 0;                                                  \
	} while (0)

#endif
