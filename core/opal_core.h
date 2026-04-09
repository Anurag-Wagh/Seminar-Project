/**
 * opal_core.h — TCG OPAL Protocol Core — Public API
 *
 * OS-agnostic header. Zero Linux headers. Zero FreeRTOS headers.
 * Allowed dependencies: opal_ral.h, opal_transport.h, stdint.h, stddef.h
 *
 * LINUX EQUIVALENT: include/linux/sed-opal.h + parts of sed-opal.c
 *
 * Key structs REMOVED vs Linux version:
 *   struct sed_opal_dev    → replaced by opal_dev_t (opaque, no kernel fields)
 *   struct opal_lock_unlock → simplified — use opal_lock_range() directly
 *   ioctl command numbers  → not needed in embedded environment
 *
 * Member 1 owns this file and opal_core.c
 */

#ifndef OPAL_CORE_H
#define OPAL_CORE_H

#include <stdint.h>
#include <stddef.h>
#include "../ral/opal_ral.h"
#include "../transport/opal_transport.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Return codes ────────────────────────────────────────────────── */
#define OPAL_OK              0   /* success                          */
#define OPAL_ERR_TRANSPORT  -1   /* send/recv failed                 */
#define OPAL_ERR_NO_MEM     -2   /* opal_alloc returned NULL         */
#define OPAL_ERR_PROTO      -3   /* unexpected TCG response/token    */
#define OPAL_ERR_AUTH       -4   /* authentication rejected by drive */
#define OPAL_ERR_LOCKED     -5   /* range already in requested state */
#define OPAL_ERR_PARAM      -6   /* bad argument (NULL, out of range)*/
#define OPAL_ERR_TIMEOUT    -7   /* operation timed out              */

/* ── Discovery result ────────────────────────────────────────────── */
/**
 * opal_discovery_t — capabilities reported by the drive during Level 0 discovery
 *
 * LINUX EQUIVALENT: fields scattered across struct opal_dev in sed-opal.c
 * (base_com_id, comid_ext, supported_func, etc.)
 * Consolidated here for clarity and ease of inspection.
 */
typedef struct {
    uint8_t  opal_v1_supported;    /* drive supports OPAL SSC v1       */
    uint8_t  opal_v2_supported;    /* drive supports OPAL SSC v2       */
    uint16_t base_com_id;          /* first ComID for session commands  */
    uint16_t num_com_ids;          /* number of ComIDs available        */
    uint8_t  locking_supported;    /* Locking feature descriptor found  */
    uint8_t  locking_enabled;      /* locking is currently active       */
    uint8_t  locked;               /* at least one range is locked      */
    uint8_t  mbr_enabled;
    uint8_t  mbr_shadow_present;
} opal_discovery_t;

/* ── Opaque device handle ────────────────────────────────────────── */
/**
 * opal_dev_t — private context for one OPAL-capable drive
 *
 * LINUX EQUIVALENT: struct opal_dev (defined inline in sed-opal.c)
 *
 * Linux struct opal_dev contains OS-specific fields:
 *   struct request_queue *queue  — kernel block I/O queue (REMOVED)
 *   struct bio           *bio    — kernel I/O descriptor  (REMOVED)
 *   wait_queue_head_t     wq     — kernel wait queue      (REMOVED)
 *   struct mutex          lock   — kernel mutex            (REPLACED by RAL)
 *   struct gendisk       *disk   — kernel block device     (REMOVED)
 *
 * Our version contains only OPAL protocol state +
 * opal_transport_t (Member 3) + opal_mutex_t (Member 2 RAL).
 * Full struct defined in opal_core.c (intentionally opaque here).
 */
typedef struct opal_dev opal_dev_t;

/* ── Lifecycle ───────────────────────────────────────────────────── */

/**
 * opal_dev_init — allocate and initialise a device context
 *
 * @transport: fully populated transport backend (from Member 3)
 * Returns: pointer on success, NULL on failure
 *
 * LINUX WAS: kzalloc + mutex_init + init_waitqueue_head
 */
opal_dev_t *opal_dev_init(const opal_transport_t *transport);

/**
 * opal_dev_destroy — release all resources
 *
 * Closes any open session before freeing.
 * LINUX WAS: mutex_destroy + kfree
 */
void opal_dev_destroy(opal_dev_t *dev);

/* ── Core OPAL operations ────────────────────────────────────────── */

/**
 * opal_discover — Level 0 Discovery
 *
 * Must be called first. Reads and stores the drive's capabilities.
 * Populates *out if non-NULL.
 *
 * LINUX EQUIVALENT: opal_discovery0() + opal_discovery0_end()
 */
int opal_discover(opal_dev_t *dev, opal_discovery_t *out);

/**
 * opal_start_admin_session — open a session with the AdminSP
 *
 * @pin / @pin_len: Admin1 PIN. Pass NULL / 0 for unauthenticated session
 *                  (valid on factory-fresh drives for Activate only).
 *
 * LINUX EQUIVALENT: opal_start_auth_session() targeting AdminSP
 */
int opal_start_admin_session(opal_dev_t *dev,
                              const uint8_t *pin, size_t pin_len);

/**
 * opal_start_locking_session — open a session with the LockingSP
 *
 * @user_id: 0 = Admin1, 1..N = User1..UserN
 * @pin / @pin_len: PIN for that user
 *
 * LINUX EQUIVALENT: opal_start_auth_session() targeting LockingSP
 */
int opal_start_locking_session(opal_dev_t *dev, uint8_t user_id,
                                const uint8_t *pin, size_t pin_len);

/**
 * opal_end_session — gracefully close the current session
 *
 * LINUX EQUIVALENT: opal_end_session()
 */
int opal_end_session(opal_dev_t *dev);

/**
 * opal_lock_range — set a locking range to Locked state
 *
 * @range_id: 0 = GlobalRange (whole disk), 1..N = LockingRange N
 * Requires an open LockingSP session.
 *
 * LINUX EQUIVALENT: opal_lock_unlock() with OPAL_LK
 */
int opal_lock_range(opal_dev_t *dev, uint8_t range_id);

/**
 * opal_unlock_range — set a locking range to Unlocked state
 *
 * @range_id: 0 = GlobalRange, 1..N = LockingRange N
 * Requires an open LockingSP session.
 *
 * LINUX EQUIVALENT: opal_lock_unlock() with OPAL_RW
 */
int opal_unlock_range(opal_dev_t *dev, uint8_t range_id);

/**
 * opal_set_password — change the PIN for a user
 *
 * @user_id:     0 = Admin1, 1..N = UserN
 * @new_pin:     new PIN bytes (max OPAL_MAX_PIN_LEN)
 * @new_pin_len: length of new PIN
 * Requires an open LockingSP session authenticated as Admin1.
 *
 * LINUX EQUIVALENT: opal_set_new_pw()
 */
int opal_set_password(opal_dev_t *dev, uint8_t user_id,
                      const uint8_t *new_pin, size_t new_pin_len);

/**
 * opal_activate_locking_sp — activate LockingSP (first-time setup only)
 *
 * Transitions LockingSP from Manufactured-Inactive to Manufactured.
 * Call once during initial drive provisioning.
 * Requires an open AdminSP session (unauthenticated on factory drive).
 *
 * LINUX EQUIVALENT: opal_activate()
 */
int opal_activate_locking_sp(opal_dev_t *dev);

/**
 * opal_revert_tper — factory reset — DESTRUCTIVE — all data lost
 *
 * @psid / @psid_len: Physical SID, printed on the drive label
 * Requires the drive to NOT be in a locked-out state.
 *
 * LINUX EQUIVALENT: opal_reverttper()
 */
int opal_revert_tper(opal_dev_t *dev,
                     const uint8_t *psid, size_t psid_len);

/**
 * opal_query_lock_state — read back current lock state of a range
 *
 * @range_id:     range to query (0 = GlobalRange)
 * @read_locked:  output — 1 if read operations are blocked
 * @write_locked: output — 1 if write operations are blocked
 * Requires an open session.
 *
 * LINUX EQUIVALENT: uses Get method on Locking table (no direct equivalent)
 */
int opal_query_lock_state(opal_dev_t *dev, uint8_t range_id,
                           uint8_t *read_locked, uint8_t *write_locked);

/* ── Diagnostics ─────────────────────────────────────────────────── */

/**
 * opal_print_discovery — log a human-readable discovery summary
 * Uses OPAL_INFO() — routes to UART/semihosting via RAL.
 */
void opal_print_discovery(const opal_discovery_t *d);

/**
 * opal_error_str — convert error code to human-readable string
 */
const char *opal_error_str(int err);

/**
 * opal_dev_get_discovery — get discovery results from device context
 * Returns NULL if dev is NULL or discovery not performed.
 */
const opal_discovery_t *opal_dev_get_discovery(const opal_dev_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* OPAL_CORE_H */
