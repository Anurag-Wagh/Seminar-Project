/* opal_ral_posix.c — POSIX implementation for desktop unit testing */

#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include "opal_ral.h"

/* Standard C / POSIX headers — only allowed in this file, never in core */
#include <stdlib.h>      /* malloc, free                */
#include <pthread.h>     /* pthread_mutex_*             */
#include <unistd.h>      /* usleep                      */
#include <stdio.h>       /* printf, vsnprintf           */
#include <stdarg.h>      /* va_list                     */
#include <time.h>        /* clock_gettime               */

/* ─── Memory ────────────────────────────────────────────────────────────── */

void *opal_alloc(size_t size)
{
    return malloc(size);
}

void opal_free(void *ptr)
{
    free(ptr);
}

/* ─── Mutex ─────────────────────────────────────────────────────────────── */

opal_mutex_t opal_mutex_create(void)
{
    pthread_mutex_t *m = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    if (!m) return NULL;
    pthread_mutex_init(m, NULL);
    return (opal_mutex_t)m;
}

void opal_mutex_lock(opal_mutex_t m)
{
    pthread_mutex_lock((pthread_mutex_t *)m);
}

void opal_mutex_unlock(opal_mutex_t m)
{
    pthread_mutex_unlock((pthread_mutex_t *)m);
}

void opal_mutex_destroy(opal_mutex_t m)
{
    pthread_mutex_destroy((pthread_mutex_t *)m);
    free(m);
}

/* ─── Timing ────────────────────────────────────────────────────────────── */

void opal_sleep_ms(uint32_t ms)
{
    usleep((useconds_t)ms * 1000u);
}

uint32_t opal_uptime_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)((uint64_t)ts.tv_sec * 1000u +
                      (uint64_t)ts.tv_nsec / 1000000u);
}

/* ─── Logging ───────────────────────────────────────────────────────────── */

static const char *level_str(int level)
{
    switch (level) {
    case OPAL_LOG_ERR:  return "\033[31mERR \033[0m";
    case OPAL_LOG_WARN: return "\033[33mWARN\033[0m";
    case OPAL_LOG_INFO: return "\033[32mINFO\033[0m";
    case OPAL_LOG_DBG:  return "\033[36mDBG \033[0m";
    default:            return "????";
    }
}

void opal_log_write(int level, const char *tag, const char *msg)
{
    printf("[%s][%s] %s\n", level_str(level), tag, msg);
}
