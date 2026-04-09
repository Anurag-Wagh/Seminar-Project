/**
 * opal_transport_hw.h — Hardware ATA/NVMe Transport Interface
 *
 * Member 3 implements opal_transport_hw.c to back this header.
 * Member 4 calls opal_transport_hw_init() from the FreeRTOS task.
 */

#ifndef OPAL_TRANSPORT_HW_H
#define OPAL_TRANSPORT_HW_H

#include "opal_transport.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * opal_transport_hw_init — initialise the hardware storage controller
 *
 * Sets up the storage bus (SATA/NVMe/SD) and populates *t with the
 * send/recv callbacks that issue ATA TRUSTED SEND / TRUSTED RECEIVE
 * commands to the physical SED.
 *
 * @t: output — populated with hardware callbacks on success
 * Returns 0 on success, negative on failure.
 */
int opal_transport_hw_init(opal_transport_t *t);

/**
 * opal_transport_hw_deinit — release hardware resources
 */
void opal_transport_hw_deinit(opal_transport_t *t);

#ifdef __cplusplus
}
#endif

#endif /* OPAL_TRANSPORT_HW_H */
