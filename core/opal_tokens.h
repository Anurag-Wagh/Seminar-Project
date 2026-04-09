/* opal_tokens.h — OPAL packet and token constants */
#ifndef OPAL_TOKENS_H
#define OPAL_TOKENS_H

#include <stdint.h>

/* ── Packet header sizes (bytes) ──────────────────────────── */
#define OPAL_COMPACKET_HDR_LEN    20
#define OPAL_PACKET_HDR_LEN       24
#define OPAL_SUBPACKET_HDR_LEN     8
#define OPAL_TOTAL_HDR_LEN        (OPAL_COMPACKET_HDR_LEN + \
                                    OPAL_PACKET_HDR_LEN    + \
                                    OPAL_SUBPACKET_HDR_LEN)

/* ── TCG token types (Core Spec §3.3.2) ───────────────────── */
#define OPAL_CALL              0xF8
#define OPAL_ENDOFDATA         0xF9
#define OPAL_ENDOFSESSION      0xFA
#define OPAL_STARTTRANSACTION  0xFB
#define OPAL_ENDTRANSACTION    0xFC
#define OPAL_EMPTYATOM         0xFF
#define OPAL_STARTLIST         0xF0
#define OPAL_ENDLIST           0xF1
#define OPAL_STARTNAME         0xF2
#define OPAL_ENDNAME           0xF3

/* ── UID length constants ──────────────────────────────────── */
#define OPAL_UID_LEN       8
#define OPAL_METHOD_LEN    8
#define OPAL_MAX_PIN_LEN   32

/* ── Object UIDs — use as: static const uint8_t x[] = OPAL_UID_SMUID
   Each expands to a brace-initializer list of 8 bytes ────── */
#define OPAL_UID_SMUID      0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF
#define OPAL_UID_THISSP     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01
#define OPAL_UID_ADMINSP    0x00,0x00,0x02,0x05,0x00,0x00,0x00,0x01
#define OPAL_UID_LOCKINGSP  0x00,0x00,0x02,0x05,0x00,0x00,0x00,0x02
#define OPAL_UID_GLOBALRANGE 0x00,0x00,0x08,0x02,0x00,0x00,0x00,0x01
#define OPAL_UID_LOCKINGRANGE1 0x00,0x00,0x08,0x02,0x00,0x03,0x00,0x01

/* ── Method UIDs ───────────────────────────────────────────── */
#define OPAL_UID_METHOD_STARTSESSION 0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x02
#define OPAL_UID_METHOD_REVERT       0x00,0x00,0x00,0x06,0x00,0x00,0x02,0x02
#define OPAL_UID_METHOD_ACTIVATE     0x00,0x00,0x00,0x06,0x00,0x00,0x02,0x03
#define OPAL_UID_METHOD_GET          0x00,0x00,0x00,0x06,0x00,0x00,0x00,0x16
#define OPAL_UID_METHOD_SET          0x00,0x00,0x00,0x06,0x00,0x00,0x00,0x17
#define OPAL_UID_METHOD_AUTHENTICATE 0x00,0x00,0x00,0x06,0x00,0x00,0x00,0x1C
#define OPAL_UID_METHOD_ENDSESSION   0x00,0x00,0x00,0x06,0x00,0x00,0xFF,0x06

/* ── Authority UIDs ────────────────────────────────────────── */
#define OPAL_UID_AUTH_ANYBODY  0x00,0x00,0x00,0x09,0x00,0x00,0x00,0x01
#define OPAL_UID_AUTH_ADMIN1   0x00,0x00,0x00,0x09,0x00,0x01,0x00,0x01
#define OPAL_UID_AUTH_USER1    0x00,0x00,0x00,0x09,0x00,0x03,0x00,0x01
#define OPAL_UID_AUTH_PSID     0x00,0x00,0x00,0x09,0x00,0x01,0xFF,0x01

/* ── Level 0 Discovery feature codes ──────────────────────── */
#define OPAL_FEAT_TPER          0x0001
#define OPAL_FEAT_LOCKING       0x0002
#define OPAL_FEAT_GEOMETRY      0x0003
#define OPAL_FEAT_OPALV1        0x0200
#define OPAL_FEAT_OPALV2        0x0203
#define OPAL_FEAT_SINGLE_USER   0x0201
#define OPAL_FEAT_DATASTORE     0x0202

/* ── Locking table column IDs ──────────────────────────────── */
#define OPAL_COL_READ_LOCK_ENABLED   5
#define OPAL_COL_WRITE_LOCK_ENABLED  6
#define OPAL_COL_READ_LOCKED         7
#define OPAL_COL_WRITE_LOCKED        8

/* ── Drive status codes ────────────────────────────────────── */
#define OPAL_STATUS_SUCCESS              0x00
#define OPAL_STATUS_NOT_AUTHORIZED       0x01
#define OPAL_STATUS_INVALID_PARAM        0x05
#define OPAL_STATUS_TPER_MALFUNCTION     0x1F
#define OPAL_STATUS_TRANSACTION_FAIL     0x40
#define OPAL_STATUS_RESPONSE_OVERFLOW    0x41
#define OPAL_STATUS_AUTHORITY_LOCKED_OUT 0x2E

/* ── Fixed ComID for Level 0 discovery ─────────────────────── */
#define OPAL_DISCOVERY_COMID   0x0001

#endif /* OPAL_TOKENS_H */
