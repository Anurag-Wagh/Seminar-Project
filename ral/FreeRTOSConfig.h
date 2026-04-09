/**
 * FreeRTOSConfig.h — FreeRTOS Configuration for OPAL SED Management
 *
 * This template targets a Cortex-M4/M7 microcontroller (e.g. STM32F4xx,
 * STM32H7xx) running at 168 MHz. Adjust the values marked [PLATFORM]
 * for your specific hardware.
 *
 * Settings marked [OPAL REQUIRED] must not be changed — the OPAL RAL
 * depends on them.
 *
 * Member 4 owns this file (integration + build).
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* ── Scheduler behaviour ────────────────────────────────────────────────── */
#define configUSE_PREEMPTION                    1
#define configUSE_TIME_SLICING                  1
#define configUSE_TICKLESS_IDLE                 0   /* disable for simplicity */
#define configCPU_CLOCK_HZ                      168000000UL  /* [PLATFORM] Hz */
#define configSYSTICK_CLOCK_HZ                  configCPU_CLOCK_HZ

/* ── Tick rate ───────────────────────────────────────────────────────────
 * [OPAL REQUIRED] Must be >= 100.
 * 1000 Hz gives 1ms resolution for opal_sleep_ms().
 * Lower values (e.g. 100) give 10ms resolution — acceptable but coarser.
 */
#define configTICK_RATE_HZ                      ( ( TickType_t ) 1000 )

/* ── Task priorities ─────────────────────────────────────────────────────
 * [PLATFORM] Adjust to suit your application.
 * OPAL task should run at a mid-level priority.
 */
#define configMAX_PRIORITIES                    ( 7 )
#define configMINIMAL_STACK_SIZE                ( ( uint16_t ) 128 )
#define configMAX_TASK_NAME_LEN                 ( 12 )

/* ── Heap ────────────────────────────────────────────────────────────────
 * [PLATFORM] Total FreeRTOS heap in bytes.
 * OPAL needs at least: sizeof(opal_dev_t) ~800 bytes + mutex overhead ~80 bytes.
 * Recommended minimum: 8192 bytes for the OPAL task + its stack.
 */
#define configTOTAL_HEAP_SIZE                   ( ( size_t )( 16 * 1024 ) )

/* ── Memory allocation ───────────────────────────────────────────────────
 * [OPAL REQUIRED] Both must be 1.
 * pvPortMalloc / vPortFree (used by opal_alloc / opal_free) require
 * dynamic allocation to be enabled.
 */
#define configSUPPORT_DYNAMIC_ALLOCATION        1   /* [OPAL REQUIRED] */
#define configSUPPORT_STATIC_ALLOCATION         0

/* ── Mutexes ─────────────────────────────────────────────────────────────
 * [OPAL REQUIRED] Must be 1.
 * opal_mutex_create() calls xSemaphoreCreateMutex() which requires this.
 */
#define configUSE_MUTEXES                       1   /* [OPAL REQUIRED] */
#define configUSE_RECURSIVE_MUTEXES             0
#define configUSE_COUNTING_SEMAPHORES           0

/* ── Queues and notifications ────────────────────────────────────────────
 * [OPAL REQUIRED] Notifications must be enabled (used internally by semaphores).
 */
#define configUSE_TASK_NOTIFICATIONS            1   /* [OPAL REQUIRED] */
#define configTASK_NOTIFICATION_ARRAY_ENTRIES   1
#define configQUEUE_REGISTRY_SIZE               8

/* ── Run-time stats ──────────────────────────────────────────────────────
 * Optional. Enable for profiling.
 */
#define configGENERATE_RUN_TIME_STATS           0
#define configUSE_TRACE_FACILITY                0
#define configUSE_STATS_FORMATTING_FUNCTIONS    0

/* ── Co-routines ─────────────────────────────────────────────────────────
 * Not used by OPAL.
 */
#define configUSE_CO_ROUTINES                   0
#define configMAX_CO_ROUTINE_PRIORITIES         ( 2 )

/* ── Timers ──────────────────────────────────────────────────────────────
 * Software timers not required by OPAL core but may be used by app layer.
 */
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               ( 2 )
#define configTIMER_QUEUE_LENGTH                10
#define configTIMER_TASK_STACK_DEPTH            ( configMINIMAL_STACK_SIZE * 2 )

/* ── Idle task behaviour ─────────────────────────────────────────────────
 */
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configUSE_DAEMON_TASK_STARTUP_HOOK      0
#define configIDLE_SHOULD_YIELD                 1

/* ── Stack overflow detection ────────────────────────────────────────────
 * [PLATFORM] Recommended during development.
 * Mode 2 = check entire stack, Mode 1 = check watermark only.
 */
#define configCHECK_FOR_STACK_OVERFLOW          2

/* ── malloc failure hook ─────────────────────────────────────────────────
 * [PLATFORM] Define vApplicationMallocFailedHook() in your BSP to handle
 * heap exhaustion (e.g. log an error, halt, or restart).
 */
#define configUSE_MALLOC_FAILED_HOOK            1

/* ── Assert ──────────────────────────────────────────────────────────────
 * [PLATFORM] Replace with your platform's assert if needed.
 */
#include <assert.h>
#define configASSERT( x ) assert( x )

/* ── Logging ─────────────────────────────────────────────────────────────
 * [PLATFORM] Define configPRINTF to route OPAL logs to your UART.
 * Example for STM32 with ITM:
 *   #define configPRINTF( X )  vLoggingPrintf X
 *
 * If left undefined, opal_ral_freertos.c falls back to
 * opal_platform_log_write() which you implement in your BSP.
 */
/* #define configPRINTF( X )   vLoggingPrintf X */

/* ── API inclusion ───────────────────────────────────────────────────────
 * Include all standard FreeRTOS API functions.
 */
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_xResumeFromISR                  1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1   /* [OPAL REQUIRED] */
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_xTaskGetCurrentTaskHandle       1
#define INCLUDE_uxTaskGetStackHighWaterMark     1
#define INCLUDE_xTaskGetIdleTaskHandle          0
#define INCLUDE_eTaskGetState                   0
#define INCLUDE_xTimerPendFunctionCall          0
#define INCLUDE_xTaskAbortDelay                 0
#define INCLUDE_xTaskGetHandle                  0
#define INCLUDE_xTaskResumeFromISR              1
#define INCLUDE_xSemaphoreGetMutexHolder        1

/* ── Cortex-M interrupt priority grouping ────────────────────────────────
 * [PLATFORM] For Cortex-M3/M4/M7. Adjust for your MCU.
 * configMAX_SYSCALL_INTERRUPT_PRIORITY must be > 0.
 * Interrupt priorities above (numerically lower) this value cannot
 * call FreeRTOS API functions from ISR context.
 */
#ifdef __NVIC_PRIO_BITS
    #define configPRIO_BITS                     __NVIC_PRIO_BITS
#else
    #define configPRIO_BITS                     4
#endif

#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY         15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY    5

#define configKERNEL_INTERRUPT_PRIORITY         \
    ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )

#define configMAX_SYSCALL_INTERRUPT_PRIORITY    \
    ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )

/* ── Cortex-M exception handlers ─────────────────────────────────────────
 * Map FreeRTOS handlers to CMSIS names.
 */
#define xPortPendSVHandler      PendSV_Handler
#define vPortSVCHandler         SVC_Handler
#define xPortSysTickHandler     SysTick_Handler

#endif /* FREERTOS_CONFIG_H */
