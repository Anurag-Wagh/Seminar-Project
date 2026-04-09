/**
 * opal_app_freertos.c — FreeRTOS Application Entry Point
 *
 * This file ties all modules together and runs on the embedded target:
 *
 *   opal_core.c          ← OPAL protocol (Member 1)
 *   opal_ral_freertos.c  ← FreeRTOS OS services (Member 2)
 *   opal_transport_hw.c  ← Hardware ATA driver (Member 3)
 *   THIS FILE            ← Integration + FreeRTOS wiring (Member 4)
 *
 * HOW IT WORKS:
 *   main() initialises the hardware (clocks, UART, storage controller)
 *   then creates one FreeRTOS task — opal_task() — and starts the scheduler.
 *
 *   opal_task() demonstrates the full OPAL operation sequence:
 *     1. Discover drive capabilities
 *     2. Open an AdminSP session
 *     3. Activate the LockingSP (first-time setup only)
 *     4. End session
 *     5. Open a LockingSP session
 *     6. Lock the GlobalRange
 *     7. Unlock the GlobalRange
 *     8. End session
 *
 * PLATFORM:
 *   Target: STM32F407 (or any Cortex-M with a SATA/NVMe storage interface)
 *   RTOS:   FreeRTOS v10.x or v11.x
 *   Heap:   heap_4.c (recommended)
 *
 * Member 4 owns this file.
 */

#include "FreeRTOS.h"
#include "task.h"

/* OPAL core API — OS-agnostic */
#include "../core/opal_core.h"

/* Hardware transport — provided by Member 3 */
#include "../transport/opal_transport_hw.h"

/* ── Task configuration ─────────────────────────────────────────────────── */

#define OPAL_TASK_STACK_WORDS   512u   /* 2 KB on Cortex-M (4 bytes/word) */
#define OPAL_TASK_PRIORITY      3u     /* mid-level; below real-time tasks  */

/* ── Default PIN — replace with your actual drive PIN ────────────────────── */
static const uint8_t ADMIN_PIN[]   = { 'A','d','m','i','n','1' };
static const uint8_t LOCKING_PIN[] = { 'A','d','m','i','n','1' };

/* ── Forward declarations ───────────────────────────────────────────────── */
static void opal_task(void *params);
static void hw_init(void);
static void log_result(const char *op, int r);

/* ═══════════════════════════════════════════════════════════════════════════
 * main — hardware init + scheduler start
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(void)
{
    /* 1. Initialise clocks, UART for logging, storage controller */
    hw_init();

    /* 2. Create the OPAL demonstration task */
    BaseType_t ok = xTaskCreate(
        opal_task,
        "OpalTask",
        OPAL_TASK_STACK_WORDS,
        NULL,
        OPAL_TASK_PRIORITY,
        NULL
    );

    if (ok != pdPASS) {
        /* Task creation failed — heap too small or stack too large */
        /* Halt here during bring-up; production code should reset  */
        for (;;) { /* halt */ }
    }

    /* 3. Start the FreeRTOS scheduler — never returns */
    vTaskStartScheduler();

    /* Should never reach here */
    for (;;) { /* halt */ }
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * opal_task — main OPAL operation sequence
 *
 * Runs once, demonstrates all core OPAL operations, then deletes itself.
 * In a real product this would be an event-driven loop responding to
 * lock/unlock commands from the application.
 * ═══════════════════════════════════════════════════════════════════════════ */

static void opal_task(void *params)
{
    (void)params;

    /* ── Step 1: Initialise transport (hardware ATA driver from Member 3) ── */
    opal_transport_t transport;
    int hw_ok = opal_transport_hw_init(&transport);
    if (hw_ok != 0) {
        OPAL_ERR("Hardware transport init failed: %d", hw_ok);
        vTaskDelete(NULL);
        return;
    }
    OPAL_INFO("Storage transport initialised");

    /* ── Step 2: Initialise OPAL device context ─────────────────────────── */
    opal_dev_t *dev = opal_dev_init(&transport);
    if (dev == NULL) {
        OPAL_ERR("opal_dev_init failed — out of heap?");
        vTaskDelete(NULL);
        return;
    }
    OPAL_INFO("OPAL device context created");

    int r;

    /* ── Step 3: Level 0 Discovery ──────────────────────────────────────── */
    opal_discovery_t disc;
    r = opal_discover(dev, &disc);
    log_result("opal_discover", r);
    if (r != OPAL_OK) goto cleanup;

    OPAL_INFO("Drive: OPAL v%s  ComID=0x%04X  locking=%s  locked=%s",
              disc.opal_v2_supported ? "2" : "1",
              disc.base_com_id,
              disc.locking_enabled  ? "enabled"  : "disabled",
              disc.locked           ? "YES"       : "no");

    /* ── Step 4: First-time setup — Activate LockingSP ──────────────────── */
    if (!disc.locking_enabled) {
        OPAL_INFO("LockingSP not yet active — running first-time activation");

        r = opal_start_admin_session(dev, ADMIN_PIN, sizeof(ADMIN_PIN));
        log_result("opal_start_admin_session (activate)", r);
        if (r != OPAL_OK) goto cleanup;

        r = opal_activate_locking_sp(dev);
        log_result("opal_activate_locking_sp", r);

        opal_end_session(dev);

        if (r != OPAL_OK) goto cleanup;
        OPAL_INFO("LockingSP activated successfully");
    }

    /* ── Step 5: Open LockingSP session ─────────────────────────────────── */
    r = opal_start_locking_session(dev, 0 /* Admin1 */,
                                   LOCKING_PIN, sizeof(LOCKING_PIN));
    log_result("opal_start_locking_session", r);
    if (r != OPAL_OK) goto cleanup;

    /* ── Step 6: Lock GlobalRange ────────────────────────────────────────── */
    r = opal_lock_range(dev, 0);
    log_result("opal_lock_range(GlobalRange)", r);

    /* ── Step 7: Unlock GlobalRange ──────────────────────────────────────── */
    if (r == OPAL_OK) {
        r = opal_unlock_range(dev, 0);
        log_result("opal_unlock_range(GlobalRange)", r);
    }

    /* ── Step 8: End session ─────────────────────────────────────────────── */
    r = opal_end_session(dev);
    log_result("opal_end_session", r);

    OPAL_INFO("=== OPAL demonstration complete ===");

cleanup:
    /* ── Cleanup ─────────────────────────────────────────────────────────── */
    opal_dev_destroy(dev);
    opal_transport_hw_deinit(&transport);

    OPAL_INFO("OPAL task exiting");
    vTaskDelete(NULL);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * hw_init — platform-specific hardware initialisation
 *
 * [PLATFORM] Replace the body with your MCU's BSP calls.
 * ═══════════════════════════════════════════════════════════════════════════ */

static void hw_init(void)
{
    /*
     * For STM32 (generated by STM32CubeMX):
     *   HAL_Init();
     *   SystemClock_Config();
     *   MX_GPIO_Init();
     *   MX_USART2_UART_Init();    // for OPAL log output
     *   MX_SDMMC1_SD_Init();      // or MX_SPI1_Init() for your storage bus
     *
     * For ESP32 (ESP-IDF):
     *   esp_log_level_set("*", ESP_LOG_INFO);
     *   // storage init happens in opal_transport_hw_init()
     *
     * For bare metal with no BSP:
     *   Configure PLL/clocks manually
     *   Configure UART baud rate register
     *   Configure SATA/NVMe controller registers
     */
}

/* ═══════════════════════════════════════════════════════════════════════════
 * log_result — log success/failure for each OPAL step
 * ═══════════════════════════════════════════════════════════════════════════ */

static void log_result(const char *op, int r)
{
    if (r == OPAL_OK) {
        OPAL_INFO("%-40s  OK", op);
    } else {
        OPAL_ERR("%-40s  FAILED (err=%d)", op, r);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * FreeRTOS hook implementations — required by FreeRTOSConfig.h settings
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * vApplicationMallocFailedHook — called when pvPortMalloc returns NULL
 * [PLATFORM] Add your error handling here (log, reset watchdog, etc.)
 */
void vApplicationMallocFailedHook(void)
{
    OPAL_ERR("FreeRTOS heap exhausted — pvPortMalloc returned NULL");
    for (;;) { /* halt — inspect heap stats with uxTaskGetSystemState() */ }
}

/**
 * vApplicationStackOverflowHook — called when stack overflow is detected
 * [PLATFORM] Log the task name then halt or reset.
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    OPAL_ERR("Stack overflow in task: %s", pcTaskName);
    for (;;) { /* halt */ }
}

/**
 * vApplicationIdleHook — called from the Idle task on every idle cycle
 * [PLATFORM] Use for low-power sleep (e.g. __WFI() on Cortex-M).
 */
void vApplicationIdleHook(void)
{
    /* __WFI(); */  /* uncomment for low-power idle on Cortex-M */
}
