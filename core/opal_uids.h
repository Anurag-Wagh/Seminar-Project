/**
 * opal_uids.h — TCG OPAL UID and Method Byte Arrays
 *
 * All 8-byte UIDs and Method IDs defined as static const arrays.
 * Source: TCG Storage Architecture Core Specification v2.01
 *         TCG OPAL SSC v2.01
 *
 * ── HOW TO USE ───────────────────────────────────────────────────────────
 * #include this file in opal_core.c ONLY.
 * Never include it in a header — the static arrays would be
 * duplicated in every translation unit that included the header.
 *
 * All names use the UID_ prefix to avoid collisions with the
 * OPAL_UID_* macro names in opal_tokens.h.
 *
 * ── TCG SPEC REFERENCES ──────────────────────────────────────────────────
 * SMUID           Core Spec §5.1.3   — Session Manager object
 * AdminSP         Core Spec §5.1.5   — Administrative Security Provider
 * LockingSP       OPAL SSC §4.3.2    — Locking Security Provider
 * GlobalRange     OPAL SSC §4.3.6.1  — The global locking range (range 0)
 * LockingRange1   OPAL SSC §4.3.6.2  — Named locking range 1
 * C_PIN_Admin1    Core Spec Table 227 — Admin1 credential row
 * C_PIN_User1     Core Spec Table 227 — User1 credential row
 * StartSession    Core Spec §5.2.3   — Open a session to an SP
 * Revert          Core Spec §5.2.5   — Revert SP to factory state
 * Activate        OPAL SSC §4.3.7    — Activate LockingSP
 * Get             Core Spec §5.3.3   — Read table rows/columns
 * Set             Core Spec §5.3.4   — Write table rows/columns
 * Authenticate    Core Spec §5.3.5   — Authenticate authority
 * EndSession      Core Spec §5.2.4   — Close the current session
 *
 * Member 1 owns this file.
 */

#ifndef OPAL_UIDS_H
#define OPAL_UIDS_H

#include <stdint.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Object UIDs
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Session Manager UID — used as invoking UID for StartSession */
static const uint8_t UID_SMUID[8]          = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF};

/** ThisSP — refers to the currently open SP within a session */
static const uint8_t UID_THISSP[8]         = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01};

/** Administrative Security Provider */
static const uint8_t UID_ADMINSP[8]        = {0x00,0x00,0x02,0x05,0x00,0x00,0x00,0x01};

/** Locking Security Provider */
static const uint8_t UID_LOCKINGSP[8]      = {0x00,0x00,0x02,0x05,0x00,0x00,0x00,0x02};

/** Global locking range (range_id = 0, covers entire drive) */
static const uint8_t UID_GLOBALRANGE[8]    = {0x00,0x00,0x08,0x02,0x00,0x00,0x00,0x01};

/** LockingRange1 (range_id = 1, first named range) */
static const uint8_t UID_LOCKINGRANGE1[8]  = {0x00,0x00,0x08,0x02,0x00,0x03,0x00,0x01};

/** C_PIN_Admin1 — credential table row for the Admin1 authority */
static const uint8_t UID_CPIN_ADMIN1[8]    = {0x00,0x00,0x00,0x0B,0x00,0x01,0x00,0x01};

/** C_PIN_User1 — credential table row for the User1 authority */
static const uint8_t UID_CPIN_USER1[8]     = {0x00,0x00,0x00,0x0B,0x00,0x03,0x00,0x01};

/* ═══════════════════════════════════════════════════════════════════════════
 * Authority UIDs
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Anybody — unauthenticated authority (read-only access) */
static const uint8_t UID_AUTH_ANYBODY[8]   = {0x00,0x00,0x00,0x09,0x00,0x00,0x00,0x01};

/** Admin1 — primary administrator authority */
static const uint8_t UID_AUTH_ADMIN1[8]    = {0x00,0x00,0x00,0x09,0x00,0x01,0x00,0x01};

/** User1 — first end-user authority */
static const uint8_t UID_AUTH_USER1[8]     = {0x00,0x00,0x00,0x09,0x00,0x03,0x00,0x01};

/**
 * PSID — Physical Security ID authority
 * Used only with AdminSP.Revert[] to factory-reset the drive.
 * The PSID value itself is printed on the drive label.
 */
static const uint8_t UID_AUTH_PSID[8]      = {0x00,0x00,0x00,0x09,0x00,0x01,0xFF,0x01};

/* ═══════════════════════════════════════════════════════════════════════════
 * Method UIDs
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * StartSession — invoked on SMUID to open a session with an SP
 * Equivalent to Linux: opal_start_auth_session() → blk_execute_rq()
 */
static const uint8_t UID_STARTSESSION[8]   = {0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x02};

/**
 * EndSession — close the currently open session
 * Sent as a bare token (0xFA) in our implementation, not as a method call
 */
static const uint8_t UID_ENDSESSION[8]     = {0x00,0x00,0x00,0x06,0x00,0x00,0xFF,0x06};

/**
 * Revert — factory-reset an SP (destructive, requires PSID)
 * Equivalent to Linux: opal_reverttper()
 */
static const uint8_t UID_REVERT[8]         = {0x00,0x00,0x00,0x06,0x00,0x00,0x02,0x02};

/**
 * Activate — transition LockingSP from Manufactured-Inactive to Active
 * Equivalent to Linux: opal_activate()
 */
static const uint8_t UID_ACTIVATE[8]       = {0x00,0x00,0x00,0x06,0x00,0x00,0x02,0x03};

/**
 * Get — read rows or columns from a table
 * Used internally for lock-state queries
 */
static const uint8_t UID_GET[8]            = {0x00,0x00,0x00,0x06,0x00,0x00,0x00,0x16};

/**
 * Set — write rows or columns in a table
 * Used for: lock/unlock (Locking table), change PIN (C_PIN table)
 * Equivalent to Linux: opal_lock_unlock(), opal_set_new_passwd()
 */
static const uint8_t UID_SET[8]            = {0x00,0x00,0x00,0x06,0x00,0x00,0x00,0x17};

/**
 * Authenticate — verify an authority's credentials within a session
 * Used after StartSession to prove identity with a PIN
 */
static const uint8_t UID_AUTHENTICATE[8]   = {0x00,0x00,0x00,0x06,0x00,0x00,0x00,0x1C};

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper: build a LockingRange UID for range_id > 0
 *
 * Usage:
 *   uint8_t range_uid[8];
 *   opal_build_locking_range_uid(range_uid, range_id);
 *
 * For range_id == 0, use UID_GLOBALRANGE directly.
 * ═══════════════════════════════════════════════════════════════════════════ */
#include <string.h>

static inline void opal_build_locking_range_uid(uint8_t out[8], uint8_t range_id)
{
    if (range_id == 0) {
        memcpy(out, UID_GLOBALRANGE, 8);
    } else {
        memcpy(out, UID_LOCKINGRANGE1, 8);
        out[7] = range_id;   /* byte 7 encodes the range number */
    }
}

#endif /* OPAL_UIDS_H */
