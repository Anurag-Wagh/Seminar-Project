/**
 * opal_ral_freertos.c — FreeRTOS Implementation of the RTOS Abstraction Layer
 *
 * This is the REAL RTOS target file. It maps every function in opal_ral.h
 * to a native FreeRTOS primitive.
 *
 * ═══════════════════════════════════════════════════════════════════════════
 * MAPPING TABLE: opal_ral.h → FreeRTOS API
 * ═══════════════════════════════════════════════════════════════════════════
 *
 *  RAL function          FreeRTOS primitive          Header
 *  ─────────────────────────────────────────────────────────────────────────
 *  opal_alloc()          pvPortMalloc()              FreeRTOS.h
 *  opal_free()           vPortFree()                 FreeRTOS.h
 *  opal_mutex_create()   xSemaphoreCreateMutex()     semphr.h
 *  opal_mutex_lock()     xSemaphoreTake(portMAX_DELAY) semphr.h
 *  opal_mutex_unlock()   xSemaphoreGive()            semphr.h
 *  opal_mutex_destroy()  vSemaphoreDelete()          semphr.h
 *  opal_sleep_ms()       vTaskDelay(pdMS_TO_TICKS()) task.h
 *  opal_uptime_ms()      xTaskGetTickCount()         task.h
 *  opal_log_write()      configPRINTF() / UART        FreeRTOS config
 *
 * ═══════════════════════════════════════════════════════════════════════════
 * PORTING TO ANOTHER RTOS
 * ═══════════════════════════════════════════════════════════════════════════
 * Create opal_ral_<os>.c and implement the same 9 functions.
 * Do NOT touch opal_core.c, opal_ral.h, or any other file.
 *
 * Example mappings for other RTOSes:
 *
 *   Zephyr:   opal_alloc → k_malloc        opal_mutex_lock → k_mutex_lock
 *             opal_sleep_ms → k_sleep      opal_log_write → LOG_INF
 *
 *   ThreadX:  opal_alloc → tx_byte_allocate  opal_mutex_lock → tx_mutex_get
 *             opal_sleep_ms → tx_thread_sleep
 *
 *   Bare metal: opal_alloc → malloc (if heap available) or static pool
 *               opal_mutex_lock → disable/enable interrupts (single-core)
 *               opal_sleep_ms → busy-wait loop using hardware timer
 *
 * ═══════════════════════════════════════════════════════════════════════════
 * EMBEDDED CONSTRAINTS
 * ═══════════════════════════════════════════════════════════════════════════
 * - pvPortMalloc uses FreeRTOS heap (heap_4.c recommended for embedded)
 * - Mutex is a binary semaphore with priority inheritance (xSemaphoreCreateMutex)
 * - xSemaphoreTake uses portMAX_DELAY — blocks the calling task indefinitely
 *   until the mutex is available. Suitable for embedded; no busy-wait.
 * - vTaskDelay converts milliseconds to ticks using configTICK_RATE_HZ.
 *   Ensure configTICK_RATE_HZ >= 1000 for 1ms resolution.
 * - Logging goes through configPRINTF if defined, otherwise falls back to
 *   a weak UART stub that can be overridden per platform.
 *
 * Member 2 owns this file.
 */

#include "opal_ral.h"

/* ── FreeRTOS headers ───────────────────────────────────────────────────────
 * These are the ONLY OS-specific includes in the entire OPAL codebase.
 * All other source files include only opal_ral.h and standard C headers.
 */
#include "FreeRTOS.h"   /* pvPortMalloc, vPortFree, portMAX_DELAY, pdMS_TO_TICKS */
#include "semphr.h"     /* SemaphoreHandle_t, xSemaphoreCreateMutex, etc.        */
#include "task.h"       /* vTaskDelay, xTaskGetTickCount, TickType_t              */

#include <stdio.h>      /* vsnprintf for log formatting                           */
#include <stdarg.h>     /* va_list for log formatting                             */
#include <string.h>     /* memset                                                 */

/* ═══════════════════════════════════════════════════════════════════════════
 * COMPILE-TIME CHECKS
 * Catch common FreeRTOSConfig.h misconfigurations at build time.
 * ═══════════════════════════════════════════════════════════════════════════ */

#if configUSE_MUTEXES != 1
#error "opal_ral_freertos.c: configUSE_MUTEXES must be 1 in FreeRTOSConfig.h"
#endif

#if configSUPPORT_DYNAMIC_ALLOCATION != 1
#error "opal_ral_freertos.c: configSUPPORT_DYNAMIC_ALLOCATION must be 1"
#endif

#if (configTICK_RATE_HZ < 100)
#warning "opal_ral_freertos.c: configTICK_RATE_HZ < 100 — opal_sleep_ms resolution will be coarse"
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * SECTION 1 — Memory
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * opal_alloc — allocate heap memory via FreeRTOS heap manager
 *
 * Uses pvPortMalloc() which is thread-safe by design — it disables
 * interrupts (or uses a mutex depending on heap implementation) during
 * allocation. heap_4.c is recommended for embedded: it coalesces free
 * blocks and has good fragmentation resistance.
 *
 * LINUX WAS: kmalloc(size, GFP_KERNEL)
 */
void *opal_alloc(size_t size)
{
    void *p = pvPortMalloc(size);
    /* pvPortMalloc returns NULL if heap is exhausted — caller must check */
    return p;
}

/**
 * opal_free — release memory back to FreeRTOS heap
 *
 * LINUX WAS: kfree(ptr)
 */
void opal_free(void *ptr)
{
    if (ptr != NULL) {
        vPortFree(ptr);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * SECTION 2 — Synchronisation
 *
 * We use a FreeRTOS Mutex (xSemaphoreCreateMutex) rather than a binary
 * semaphore because mutexes include priority inheritance. This prevents
 * priority inversion when a low-priority task holds the OPAL lock while
 * a high-priority task is waiting for it.
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * opal_mutex_create — create a FreeRTOS mutex
 *
 * Returns the SemaphoreHandle_t cast to opal_mutex_t (void*).
 * Returns NULL if heap is exhausted.
 *
 * LINUX WAS: mutex_init(&dev->opal_lock)
 */
opal_mutex_t opal_mutex_create(void)
{
    SemaphoreHandle_t handle = xSemaphoreCreateMutex();
    /* xSemaphoreCreateMutex returns NULL on allocation failure */
    return (opal_mutex_t)handle;
}

/**
 * opal_mutex_lock — acquire the mutex, blocking indefinitely
 *
 * portMAX_DELAY causes the calling FreeRTOS task to block until the
 * mutex becomes available — it does NOT busy-wait or spin. The scheduler
 * will run other tasks while this one is blocked.
 *
 * LINUX WAS: mutex_lock(&dev->opal_lock)
 */
void opal_mutex_lock(opal_mutex_t m)
{
    if (m != NULL) {
        xSemaphoreTake((SemaphoreHandle_t)m, portMAX_DELAY);
    }
}

/**
 * opal_mutex_unlock — release the mutex
 *
 * LINUX WAS: mutex_unlock(&dev->opal_lock)
 */
void opal_mutex_unlock(opal_mutex_t m)
{
    if (m != NULL) {
        xSemaphoreGive((SemaphoreHandle_t)m);
    }
}

/**
 * opal_mutex_destroy — delete the mutex and free its memory
 *
 * vSemaphoreDelete frees the FreeRTOS internal semaphore structure.
 * Must only be called when no task is waiting on the mutex.
 *
 * LINUX WAS: mutex_destroy(&dev->opal_lock)
 */
void opal_mutex_destroy(opal_mutex_t m)
{
    if (m != NULL) {
        vSemaphoreDelete((SemaphoreHandle_t)m);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * SECTION 3 — Timing
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * opal_sleep_ms — suspend the calling task for at least ms milliseconds
 *
 * pdMS_TO_TICKS converts milliseconds to FreeRTOS tick count using
 * configTICK_RATE_HZ. The calling task is placed in the Blocked state
 * and the scheduler runs other tasks during this time.
 *
 * Minimum resolution = 1000 / configTICK_RATE_HZ milliseconds.
 * Example: configTICK_RATE_HZ=1000 → 1ms resolution.
 *          configTICK_RATE_HZ=100  → 10ms resolution (ms rounded up).
 *
 * LINUX WAS: msleep(ms)
 */
void opal_sleep_ms(uint32_t ms)
{
    if (ms == 0) return;
    vTaskDelay(pdMS_TO_TICKS(ms));
}

/**
 * opal_uptime_ms — milliseconds elapsed since FreeRTOS scheduler started
 *
 * xTaskGetTickCount returns the current tick count. We multiply by
 * (1000 / configTICK_RATE_HZ) to convert ticks to milliseconds.
 *
 * Note: wraps at UINT32_MAX (~49 days at 1kHz tick rate).
 * OPAL sessions are short-lived so wraparound is not a concern.
 *
 * LINUX WAS: ktime_get_ms() / jiffies_to_msecs(jiffies)
 */
uint32_t opal_uptime_ms(void)
{
    TickType_t ticks = xTaskGetTickCount();
    return (uint32_t)((uint64_t)ticks * 1000ULL / (uint64_t)configTICK_RATE_HZ);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * SECTION 4 — Logging
 *
 * Output routing depends on platform:
 *   1. If configPRINTF is defined in FreeRTOSConfig.h, use it directly.
 *      Many BSPs define configPRINTF to route to a UART or ITM trace port.
 *   2. Otherwise fall back to opal_platform_log_write() which is a weak
 *      symbol — override it in your BSP to route to your UART driver.
 *
 * LINUX WAS: pr_err() / pr_info() / pr_debug() → printk()
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Log level label strings */
static const char *const s_level_str[] = {
    "ERR ", /* OPAL_LOG_ERR  = 0 */
    "WARN", /* OPAL_LOG_WARN = 1 */
    "INFO", /* OPAL_LOG_INFO = 2 */
    "DBG " /* OPAL_LOG_DBG  = 3 */
};

/**
 * opal_platform_log_write — weak platform log backend
 *
 * Override this in your BSP file to route to your UART:
 *
 *   void opal_platform_log_write(const char *line) {
 *       HAL_UART_Transmit(&huart2, (uint8_t *)line, strlen(line), 100);
 *   }
 *
 * For STM32: use HAL_UART_Transmit or ITM_SendChar in a loop.
 * For ESP32: use esp_log or uart_write_bytes.
 * For bare metal: write directly to UART data register.
 */
__attribute__((weak)) void opal_platform_log_write(const char *line)
{
    /*
     * Default implementation: do nothing.
     * On a target with no UART configured, logs are silently dropped.
     * Replace with your platform's UART write in your BSP.
     */
    (void)line;
}

/**
 * opal_log_write — format and emit one log line
 *
 * Called by the OPAL_ERR / OPAL_INFO / OPAL_DBG macros in opal_ral.h.
 * Formats into a stack buffer (configurable size) then dispatches.
 */
void opal_log_write(int level, const char *tag, const char *msg)
{
    char buf[192];   /* fits on most embedded stacks; adjust if needed */

    /* Cap level index defensively */
    if (level < 0) level = 0;
    if (level > OPAL_LOG_DBG) level = OPAL_LOG_DBG;

    /*
     * Format: [LEVEL][TAG] message\r\n
     * \r\n because many embedded terminal emulators need CR+LF.
     */
    (void)snprintf(buf, sizeof(buf), "[%s][%s] %s\r\n",
                   s_level_str[level], tag, msg);
    buf[sizeof(buf) - 1] = '\0';   /* guarantee NUL termination */

#if defined(configPRINTF)
    /* Route through FreeRTOS BSP macro if available */
    configPRINTF(("%s", buf));
#else
    /* Fall through to weak platform backend */
    opal_platform_log_write(buf);
#endif
}

/* ═══════════════════════════════════════════════════════════════════════════
 * SECTION 5 — FreeRTOS Hooks
 *
 * These are required when configUSE_MALLOC_FAILED_HOOK=1 or
 * configCHECK_FOR_STACK_OVERFLOW=2 in FreeRTOSConfig.h.
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * vApplicationMallocFailedHook — called when pvPortMalloc fails
 *
 * FreeRTOS calls this when heap allocation fails (heap exhausted).
 * Default: log an error and halt. Override in your BSP if needed.
 */
__attribute__((weak)) void vApplicationMallocFailedHook(void)
{
    OPAL_ERR("FreeRTOS heap exhausted — halting");
    for (;;) { /* halt */ }
}

/**
 * vApplicationStackOverflowHook — called when stack overflow detected
 *
 * FreeRTOS calls this when configCHECK_FOR_STACK_OVERFLOW=2 detects
 * stack corruption. Provides task handle and name for debugging.
 *
 * Default: log an error and halt. Override in your BSP if needed.
 */
__attribute__((weak)) void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    OPAL_ERR("FreeRTOS stack overflow in task '%s' — halting", pcTaskName);
    (void)xTask;  /* unused in default impl */
    for (;;) { /* halt */ }
}
