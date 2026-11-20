#ifndef STREAM_TEST_COMMON_H
#define STREAM_TEST_COMMON_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define taskENTER_CRITICAL() ((void)0)
#define taskEXIT_CRITICAL() ((void)0)
size_t xPortGetFreeHeapSize(void);
size_t xPortGetMinimumEverFreeHeapSize(void);
#endif
