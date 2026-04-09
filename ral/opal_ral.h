/**
 * opal_ral.h — RTOS Abstraction Layer (RAL)
 *
 * This is the ONLY file that touches OS-specific concepts.
 * To port to a new OS: implement a new opal_ral_<os>.c that
 * satisfies every function declared here. Nothing else changes.
 *
 * Member 2 owns this file and opal_ral_freertos.c
 */

#ifndef OPAL_RAL_H
#define OPAL_RAL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Memory ────────────────────────────────────────────────────────────── */

/**
 * opal_alloc - allocate size bytes of memory
 * Returns pointer on success, NULL on failure.
 * Maps to: kmalloc (Linux), pvPortMalloc (FreeRTOS), malloc (POSIX)
 */
void *opal_alloc(size_t size);

/**
 * opal_free - release memory previously returned by opal_alloc
 */
void opal_free(void *ptr);

/* ─── Synchronisation ───────────────────────────────────────────────────── */

typedef void *opal_mutex_t;

/**
 * opal_mutex_create - allocate and initialise a new mutex
 * Returns handle on success, NULL on failure.
 */
opal_mutex_t opal_mutex_create(void);

/**
 * opal_mutex_lock - acquire the mutex (blocks until available)
 */
void opal_mutex_lock(opal_mutex_t m);

/**
 * opal_mutex_unlock - release the mutex
 */
void opal_mutex_unlock(opal_mutex_t m);

/**
 * opal_mutex_destroy - release all resources associated with the mutex
 */
void opal_mutex_destroy(opal_mutex_t m);

/* ─── Timing ────────────────────────────────────────────────────────────── */

/**
 * opal_sleep_ms - suspend the calling task/thread for at least ms milliseconds
 * Maps to: msleep (Linux), vTaskDelay (FreeRTOS), usleep (POSIX)
 */
void opal_sleep_ms(uint32_t ms);

/**
 * opal_uptime_ms - return milliseconds since system boot (used for timeouts)
 */
uint32_t opal_uptime_ms(void);

/* ─── Logging ───────────────────────────────────────────────────────────── */

/* Log levels */
#define OPAL_LOG_ERR   0
#define OPAL_LOG_WARN  1
#define OPAL_LOG_INFO  2
#define OPAL_LOG_DBG   3

#ifndef OPAL_LOG_LEVEL
#define OPAL_LOG_LEVEL OPAL_LOG_INFO
#endif

/**
 * opal_log_write - backend log emitter (implement per platform)
 * level: one of OPAL_LOG_* constants
 * tag:   short module name, e.g. "OPAL-CORE"
 * msg:   formatted message string (already formatted by macros below)
 */
void opal_log_write(int level, const char *tag, const char *msg);

/* Convenience macros — Member 1 and 3 use these, never opal_log_write directly */
#include <stdio.h>
#define _OPAL_LOG(lvl, tag, ...)                                \
    do {                                                        \
        if ((lvl) <= OPAL_LOG_LEVEL) {                         \
            char _buf[256];                                     \
            snprintf(_buf, sizeof(_buf), __VA_ARGS__);         \
            opal_log_write(lvl, tag, _buf);                    \
        }                                                       \
    } while (0)

#define OPAL_ERR(...)  _OPAL_LOG(OPAL_LOG_ERR,  "OPAL", __VA_ARGS__)
#define OPAL_WARN(...) _OPAL_LOG(OPAL_LOG_WARN, "OPAL", __VA_ARGS__)
#define OPAL_INFO(...) _OPAL_LOG(OPAL_LOG_INFO, "OPAL", __VA_ARGS__)
#define OPAL_DBG(...)  _OPAL_LOG(OPAL_LOG_DBG,  "OPAL", __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* OPAL_RAL_H */
