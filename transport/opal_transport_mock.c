/**
 * opal_transport_mock.c — Mock Storage Transport for Unit Testing
 *
 * Replays pre-recorded OPAL response packets. Byte layout verified
 * against the parsers in opal_core.c.
 *
 * parse_session_open() scans payload for:
 *   STARTLIST(F0) + 0x84 + HSN[4] + 0x84 + TSN[4]
 *
 * parse_status() scans backward for ENDOFDATA(F9) then reads
 * status byte at offset -4 from it.
 */
#include "opal_transport_mock.h"
#include "../core/opal_tokens.h"
#include <string.h>
#include <stdio.h>

/* Level 0 Discovery: OPAL v2, base_com_id=0x0001, locking enabled+locked */
static const uint8_t RESP_DISC[OPAL_MAX_RESP_LEN] = {
    0x00,0x00,0x00,0x48, 0x00,0x00,0x00,0x00,     /* length + reserved     */
    0x00,0x02, 0x10, 0x0C,                         /* Locking feature hdr   */
    0x07, 0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
    0x02,0x03, 0x10, 0x0C,                         /* OPAL v2 feature hdr   */
    0x00,0x01, 0x00,0x01,                          /* base_com_id, num      */
    0x00,0x00, 0x00,0x00, 0x00,0x00,0x00,0x00,
};

/*
 * StartSession success response.
 * Payload at byte 52 (OPAL_TOTAL_HDR_LEN = 20+24+8):
 *   F8 A8 <SMUID 8 bytes>     = CALL on SMUID
 *   F0                         = STARTLIST  ← parser anchors here
 *   84 00 00 00 41             = HSN echo (0x00000041)
 *   84 DE AD BE EF             = TSN       (0xDEADBEEF)
 *   F1 F9 F0 00 00 00 F1       = ENDLIST ENDOFDATA [SUCCESS]
 */
static const uint8_t RESP_SES[OPAL_MAX_RESP_LEN] = {
    /* ComPacket hdr (20) */
    0x00,0x00,0x00,0x00, 0x00,0x00, 0x00,0x01,
    0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x40,
    /* Packet hdr (24) */
    0xDE,0xAD,0xBE,0xEF, 0x00,0x00,0x00,0x41,
    0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x20,
    /* SubPacket hdr (8) */
    0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x18,
    /* Payload starts here (byte 52) */
    0xF8,                                    /* CALL                  */
    0xA8,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x02, /* SMUID.StartSession */
    0xF0,                                    /* STARTLIST             */
    0x84,0x00,0x00,0x00,0x41,               /* HSN = 0x00000041      */
    0x84,0xDE,0xAD,0xBE,0xEF,               /* TSN = 0xDEADBEEF      */
    0xF1,                                    /* ENDLIST               */
    0xF9,                                    /* ENDOFDATA             */
    0xF0,0x00,0x00,0x00,0xF1,               /* status SUCCESS        */
};

/* Generic method success: ENDOFDATA then [F0 00 00 00 F1] status list */
static const uint8_t RESP_OK[OPAL_MAX_RESP_LEN] = {
    0x00,0x00,0x00,0x00, 0x00,0x00, 0x00,0x01,
    0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x1C,
    0xDE,0xAD,0xBE,0xEF, 0x00,0x00,0x00,0x41,
    0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x0C,
    0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x06,
    /* Payload */
    0xF9,             /* ENDOFDATA */
    0xF0,             /* STARTLIST */
    0x00,             /* status = SUCCESS */
    0x00, 0x00,       /* reserved */
    0xF1,             /* ENDLIST  */
};

/* Auth failure: status = 0x01 Not Authorized */
static const uint8_t RESP_AUTHFAIL[OPAL_MAX_RESP_LEN] = {
    0x00,0x00,0x00,0x00, 0x00,0x00, 0x00,0x01,
    0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x1C,
    0xDE,0xAD,0xBE,0xEF, 0x00,0x00,0x00,0x41,
    0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x0C,
    0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x06,
    0xF9, 0xF0,
    0x01,             /* status = NOT_AUTHORIZED */
    0x00, 0x00, 0xF1,
};

typedef struct { int next_op; uint32_t sends; uint32_t recvs; } mock_ctx_t;
static mock_ctx_t g_ctx;

static int mock_send(uint8_t proto_id, uint16_t com_id,
                     const uint8_t *buf, size_t len, void *ctx)
{
    mock_ctx_t *m = (mock_ctx_t *)ctx;
    m->sends++;
    (void)buf;
    printf("[MOCK] IF-SEND proto=0x%02X com_id=0x%04X len=%u\n",
           proto_id, com_id, (unsigned)len);
    if (com_id == OPAL_DISCOVERY_COMID && len <= 56)
        m->next_op = MOCK_NEXT_DISCOVERY;
    return 0;
}

static int mock_recv(uint8_t proto_id, uint16_t com_id,
                     uint8_t *buf, size_t len, void *ctx)
{
    mock_ctx_t *m = (mock_ctx_t *)ctx;
    m->recvs++;
    (void)proto_id; (void)com_id;
    const uint8_t *src = RESP_OK;
    int prev = m->next_op;
    switch (m->next_op) {
    case MOCK_NEXT_DISCOVERY:  src = RESP_DISC;     m->next_op = MOCK_NEXT_SES_OPEN; break;
    case MOCK_NEXT_SES_OPEN:   src = RESP_SES;      m->next_op = MOCK_NEXT_SUCCESS;  break;
    case MOCK_NEXT_AUTH_FAIL:  src = RESP_AUTHFAIL; m->next_op = MOCK_NEXT_SUCCESS;  break;
    default:                   src = RESP_OK;        break;
    }
    size_t n = (OPAL_MAX_RESP_LEN < len) ? OPAL_MAX_RESP_LEN : len;
    memcpy(buf, src, n);
    printf("[MOCK] IF-RECV → %u bytes (was op=%d)\n", (unsigned)n, prev);
    return (int)n;
}

opal_transport_t mock_transport_init(void)
{
    memset(&g_ctx, 0, sizeof(g_ctx));
    g_ctx.next_op = MOCK_NEXT_DISCOVERY;
    opal_transport_t t;
    t.send = mock_send;
    t.recv = mock_recv;
    t.ctx  = &g_ctx;
    return t;
}

void     mock_transport_set_next(int op) { g_ctx.next_op = op; }
uint32_t mock_transport_send_count(void) { return g_ctx.sends; }
uint32_t mock_transport_recv_count(void) { return g_ctx.recvs; }
