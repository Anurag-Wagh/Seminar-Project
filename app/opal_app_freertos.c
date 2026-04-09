#include "FreeRTOS.h"
#include "task.h"
#include "opal_core.h"
#include "opal_transport_hw.h"

static void opal_main_task(void *params)
{
    (void)params;

    opal_transport_t transport;
    if (opal_transport_hw_init(&transport) != 0) {
        OPAL_ERR("opal_transport_hw_init failed");
        vTaskDelete(NULL);
        return;
    }

    opal_dev_t *dev = opal_dev_init(&transport);
    if (!dev) {
        OPAL_ERR("opal_dev_init failed");
        opal_transport_hw_deinit(&transport);
        vTaskDelete(NULL);
        return;
    }

    int rc = opal_discover(dev, NULL);
    if (rc != OPAL_OK) {
        OPAL_ERR("opal_discover failed: %s", opal_error_str(rc));
    } else {
        const opal_discovery_t *disc = opal_dev_get_discovery(dev);
        if (disc) {
            opal_print_discovery(disc);
        }
        rc = opal_start_admin_session(dev, NULL, 0);
        if (rc != OPAL_OK) {
            OPAL_ERR("opal_start_admin_session failed: %s", opal_error_str(rc));
        } else {
            OPAL_INFO("Admin session opened successfully");
            opal_activate_locking_sp(dev);
            opal_end_session(dev);
        }
    }

    opal_dev_destroy(dev);
    opal_transport_hw_deinit(&transport);
    vTaskDelete(NULL);
}

int main(void)
{
    BaseType_t ok = xTaskCreate(opal_main_task,
                                "OPAL",
                                configMINIMAL_STACK_SIZE * 4,
                                NULL,
                                tskIDLE_PRIORITY + 1,
                                NULL);
    if (ok != pdPASS) {
        OPAL_ERR("Failed to create OPAL task");
    }

    vTaskStartScheduler();

    for (;;) {
        /* Should never reach here */
    }
}
