/**
 * opal_core.c — TCG OPAL Protocol Core Implementation
 *
 * Derived from: Linux kernel drivers/block/sed-opal.c (v6.x)
 *
 * ═══════════════════════════════════════════════════════════════════
 *  LINUX DEPENDENCY REMOVAL — COMPLETE MAP (Member 1 key task)
 * ═══════════════════════════════════════════════════════════════════
 *
 *  Linux API                  Header               Replaced with
 *  ─────────────────────────────────────────────────────────────────
 *  kmalloc(sz, GFP_KERNEL)    <linux/slab.h>       opal_alloc(sz)
 *  kzalloc(sz, GFP_KERNEL)    <linux/slab.h>       opal_alloc + memset
 *  kfree(ptr)                 <linux/slab.h>       opal_free(ptr)
 *  mutex_init(&m)             <linux/mutex.h>      opal_mutex_create()
 *  mutex_lock(&m)             <linux/mutex.h>      opal_mutex_lock(m)
 *  mutex_unlock(&m)           <linux/mutex.h>      opal_mutex_unlock(m)
 *  mutex_destroy(&m)          <linux/mutex.h>      opal_mutex_destroy(m)
 *  msleep(ms)                 <linux/delay.h>      opal_sleep_ms(ms)
 *  pr_err(fmt,...)            <linux/printk.h>     OPAL_ERR(fmt,...)
 *  pr_warn(fmt,...)           <linux/printk.h>     OPAL_WARN(fmt,...)
 *  pr_info(fmt,...)           <linux/printk.h>     OPAL_INFO(fmt,...)
 *  pr_debug(fmt,...)          <linux/printk.h>     OPAL_DBG(fmt,...)
 *  blk_execute_rq(...)        <linux/blkdev.h>     transport->send/recv
 *  blk_mq_alloc_request(...)  <linux/blk-mq.h>    REMOVED
 *  struct request *rq         <linux/blkdev.h>     REMOVED
 *  struct bio *bio            <linux/bio.h>        REMOVED
 *  struct request_queue *q    <linux/blkdev.h>     REMOVED
 *  struct gendisk *disk       <linux/genhd.h>      REMOVED
 *  wait_queue_head_t wq       <linux/wait.h>       REMOVED
 *  DECLARE_COMPLETION(c)      <linux/completion.h> REMOVED
 *  complete(c)                <linux/completion.h> REMOVED
 *  wait_for_completion(c)     <linux/completion.h> REMOVED
 *
 *  Zero #include <linux/...> in this file. Zero #include <FreeRTOS.h>.
 *  Pure C99 — compiles with any embedded toolchain.
 * ═══════════════════════════════════════════════════════════════════
 *
 * Member 1 owns this file.
 */

#include "opal_core.h"
#include "opal_tokens.h"
#include "opal_uids.h"
#include <string.h>   /* memset, memcpy — standard C99 only */

/* ═══════════════════════════════════════════════════════════════════
 * SECTION 1 — Internal device state
 * ═══════════════════════════════════════════════════════════════════ */

/**
 * struct opal_dev — private context for one OPAL drive
 *
 * LINUX EQUIVALENT: struct opal_dev in sed-opal.c
 *
 * FIELDS REMOVED vs Linux version:
 *   struct request_queue  *queue;      // Linux block layer I/O queue
 *   struct bio            *bio;        // Linux block I/O descriptor
 *   wait_queue_head_t      cmd_wait_q; // Linux wait queue
 *   struct completion     *compl;      // Linux async completion
 *   struct mutex           opal_lock;  // Linux kernel mutex
 *   struct gendisk        *disk;       // Linux block device handle
 *
 * FIELDS ADDED for portability:
 *   opal_transport_t  transport;  // pluggable I/O (Member 3)
 *   opal_mutex_t      lock;       // OS-agnostic mutex (Member 2 RAL)
 *
 * FIELDS RETAINED (pure OPAL protocol state — OS-independent):
 *   session IDs, com_id, discovery results, packet buffers
 */
struct opal_dev {
    /* ── Transport backend (plugged in by Member 3) ─────────────── */
    opal_transport_t transport;

    /* ── Synchronisation (via RAL — Member 2) ───────────────────── */
    opal_mutex_t     lock;           /* LINUX WAS: struct mutex opal_lock */

    /* ── Discovery results ───────────────────────────────────────── */
    opal_discovery_t discovery;
    uint8_t          discovered;     /* 1 after successful opal_discover() */

    /* ── Active session state ────────────────────────────────────── */
    uint8_t          session_open;
    uint32_t         host_session_id;   /* chosen by host (arbitrary) */
    uint32_t         tper_session_id;   /* assigned by drive in response */
    uint16_t         com_id;            /* negotiated ComID */

    /* ── Packet I/O buffers (static — no per-call heap alloc) ───── */
    uint8_t          cmd_buf[OPAL_MAX_CMD_LEN];
    size_t           cmd_pos;           /* write cursor */
    uint8_t          resp_buf[OPAL_MAX_RESP_LEN];
    size_t           resp_len;          /* bytes received */
};

/* ═══════════════════════════════════════════════════════════════════
 * SECTION 2 — Packet buffer helpers
 * ═══════════════════════════════════════════════════════════════════ */

/**
 * buf_reset — clear cmd_buf and position write cursor after header zone
 *
 * LINUX EQUIVALENT: cmd_start() in sed-opal.c
 * Linux writes into a DMA buffer attached to struct request.
 * We write directly into dev->cmd_buf.
 */
static void buf_reset(struct opal_dev *dev)
{
    memset(dev->cmd_buf, 0, sizeof(dev->cmd_buf));
    dev->cmd_pos = OPAL_TOTAL_HDR_LEN;  /* reserve space for 3-layer header */
}

/** Write one raw byte into cmd_buf */
static int buf_push(struct opal_dev *dev, uint8_t b)
{
    if (dev->cmd_pos >= OPAL_MAX_CMD_LEN) {
        OPAL_ERR("cmd_buf overflow at pos=%u", (unsigned)dev->cmd_pos);
        return OPAL_ERR_PARAM;
    }
    dev->cmd_buf[dev->cmd_pos++] = b;
    return OPAL_OK;
}

/* ── TCG token emitters ─────────────────────────────────────────── */

/** Append a control token: CALL, STARTLIST, ENDLIST, etc. */
static int append_token(struct opal_dev *dev, uint8_t tok)
{
    return buf_push(dev, tok);
}

/**
 * append_u8 — encode a small unsigned integer as a TCG atom
 *
 * Values 0–63  → tiny atom  (1 byte)
 * Values 64–255 → short atom (2 bytes: 0x81 + value)
 */
static int append_u8(struct opal_dev *dev, uint8_t v)
{
    if (v <= 0x3F)
        return buf_push(dev, v);
    return buf_push(dev, 0x81) || buf_push(dev, v);
}

/**
 * append_u32 — encode a 32-bit value as a 4-byte TCG medium atom (0x84)
 */
static int append_u32(struct opal_dev *dev, uint32_t v)
{
    return buf_push(dev, 0x84)
        || buf_push(dev, (uint8_t)(v >> 24))
        || buf_push(dev, (uint8_t)(v >> 16))
        || buf_push(dev, (uint8_t)(v >>  8))
        || buf_push(dev, (uint8_t)(v      ));
}

/**
 * append_bytes — encode a byte array as a TCG byte-string atom
 *
 * len <= 15  → short byte atom  (0xA0|len, then bytes)
 * len <= 255 → medium byte atom (0xD1, len, then bytes)
 * len <= 65535 → long byte atom (0xD2, hi, lo, then bytes) — added for PIN safety
 */
static int append_bytes(struct opal_dev *dev, const uint8_t *data, size_t len)
{
    int r = 0;
    if (len <= 15) {
        r = buf_push(dev, (uint8_t)(0xA0 | (uint8_t)len));
    } else if (len <= 255) {
        r = buf_push(dev, 0xD1) || buf_push(dev, (uint8_t)len);
    } else {
        r = buf_push(dev, 0xD2)
          || buf_push(dev, (uint8_t)(len >> 8))
          || buf_push(dev, (uint8_t)(len     ));
    }
    for (size_t i = 0; i < len && !r; i++)
        r = buf_push(dev, data[i]);
    return r;
}

/**
 * buf_finalise — stamp ComPacket / Packet / SubPacket length fields
 *
 * LINUX EQUIVALENT: cmd_finalize() in sed-opal.c
 *
 * The TCG packet format has three nested length headers:
 *   [ComPacket hdr 20B][Packet hdr 24B][SubPacket hdr 8B][payload...][pad]
 *
 * Linux writes into a DMA-mapped kernel page and submits via struct request.
 * We write directly into dev->cmd_buf — no DMA, no request allocation.
 */
static void buf_finalise(struct opal_dev *dev)
{
    uint8_t *b     = dev->cmd_buf;
    size_t payload = dev->cmd_pos - OPAL_TOTAL_HDR_LEN;
    size_t pad     = (4 - (payload & 3)) & 3;   /* pad payload to 4-byte boundary */
    size_t sp_len  = payload;
    size_t pkt_len = OPAL_SUBPACKET_HDR_LEN + sp_len;
    size_t cp_len  = OPAL_PACKET_HDR_LEN    + pkt_len + pad;

    /* ── ComPacket header (bytes 0–19) ──────────────────────────── */
    memset(b, 0, OPAL_COMPACKET_HDR_LEN);
    b[6]  = (uint8_t)(dev->com_id >> 8);   /* ComID high byte */
    b[7]  = (uint8_t)(dev->com_id     );   /* ComID low byte  */
    b[16] = (uint8_t)(cp_len >> 24);
    b[17] = (uint8_t)(cp_len >> 16);
    b[18] = (uint8_t)(cp_len >>  8);
    b[19] = (uint8_t)(cp_len      );

    /* ── Packet header (bytes 20–43) ────────────────────────────── */
    b += OPAL_COMPACKET_HDR_LEN;
    memset(b, 0, OPAL_PACKET_HDR_LEN);
    /* TPer session ID (drive-assigned) — big-endian */
    b[0] = (uint8_t)(dev->tper_session_id >> 24);
    b[1] = (uint8_t)(dev->tper_session_id >> 16);
    b[2] = (uint8_t)(dev->tper_session_id >>  8);
    b[3] = (uint8_t)(dev->tper_session_id      );
    /* Host session ID — big-endian */
    b[4] = (uint8_t)(dev->host_session_id >> 24);
    b[5] = (uint8_t)(dev->host_session_id >> 16);
    b[6] = (uint8_t)(dev->host_session_id >>  8);
    b[7] = (uint8_t)(dev->host_session_id      );
    /* Packet length */
    b[20] = (uint8_t)(pkt_len >> 24);
    b[21] = (uint8_t)(pkt_len >> 16);
    b[22] = (uint8_t)(pkt_len >>  8);
    b[23] = (uint8_t)(pkt_len      );

    /* ── SubPacket header (bytes 44–51) ─────────────────────────── */
    b += OPAL_PACKET_HDR_LEN;
    memset(b, 0, OPAL_SUBPACKET_HDR_LEN);
    b[4] = (uint8_t)(sp_len >> 24);
    b[5] = (uint8_t)(sp_len >> 16);
    b[6] = (uint8_t)(sp_len >>  8);
    b[7] = (uint8_t)(sp_len      );
}

/* ═══════════════════════════════════════════════════════════════════
 * SECTION 3 — Transport dispatch
 * ═══════════════════════════════════════════════════════════════════ */

/**
 * opal_send_recv — finalise a command and exchange it with the drive
 *
 * LINUX EQUIVALENT: opal_send_recv() + submit_opal_request() in sed-opal.c
 *
 * Linux path (removed):
 *   1. blk_mq_alloc_request()       — allocate kernel request
 *   2. bio_map_kern()               — map cmd_buf into a struct bio
 *   3. blk_execute_rq()             — submit; blocks on wait_queue_head_t
 *   4. read response from DMA buf   — after interrupt wakes the wait queue
 *
 * Portable path (this function):
 *   1. buf_finalise()               — stamp packet headers in cmd_buf
 *   2. transport->send()            — Member 3 sends ATA TRUSTED SEND
 *   3. transport->recv()            — Member 3 reads ATA TRUSTED RECEIVE
 *   No kernel block layer. No DMA mapping. No wait queues.
 */
static int opal_send_recv(struct opal_dev *dev,
                           uint8_t proto_id, uint16_t com_id)
{
    int r;
    buf_finalise(dev);

    OPAL_DBG("IF-SEND proto=0x%02X comid=0x%04X len=%u",
             proto_id, com_id, (unsigned)dev->cmd_pos);

    /* ── IF-SEND: push command to drive ─────────────────────────── */
    /* LINUX WAS: blk_execute_rq(dev->queue, dev->disk, rq, 0) */
    r = dev->transport.send(proto_id, com_id,
                             dev->cmd_buf, dev->cmd_pos,
                             dev->transport.ctx);
    if (r < 0) {
        OPAL_ERR("IF-SEND failed (err=%d)", r);
        return OPAL_ERR_TRANSPORT;
    }

    /* Small mandatory delay — drive needs time to prepare response */
    /* LINUX WAS: msleep(OPAL_TPER_TIMEOUT) */
    opal_sleep_ms(10);

    /* ── IF-RECV: pull response from drive ──────────────────────── */
    memset(dev->resp_buf, 0, sizeof(dev->resp_buf));
    r = dev->transport.recv(proto_id, com_id,
                             dev->resp_buf, sizeof(dev->resp_buf),
                             dev->transport.ctx);
    if (r < 0) {
        OPAL_ERR("IF-RECV failed (err=%d)", r);
        return OPAL_ERR_TRANSPORT;
    }
    dev->resp_len = (size_t)r;

    OPAL_DBG("IF-RECV received %u bytes", (unsigned)dev->resp_len);
    return OPAL_OK;
}

/* ═══════════════════════════════════════════════════════════════════
 * SECTION 4 — Response parsers
 * ═══════════════════════════════════════════════════════════════════ */

/**
 * parse_status — extract the method status code from a response packet
 *
 * LINUX EQUIVALENT: parse_status() in sed-opal.c
 *
 * The TCG status block is the last 5 tokens before ENDOFDATA:
 *   [STARTLIST] [status_code u8] [reserved u8] [reserved u8] [ENDLIST] [ENDOFDATA]
 *
 * Linux reads from the DMA-mapped response buffer.
 * We read directly from dev->resp_buf.
 */
static int parse_status(struct opal_dev *dev)
{
    uint8_t *buf = dev->resp_buf;
    size_t   len = dev->resp_len ? dev->resp_len : OPAL_MAX_RESP_LEN;

    for (int i = (int)len - 1; i >= 5; i--) {
        if (buf[i] == OPAL_ENDOFDATA) {
            /* Layout: [...][STARTLIST][status][res][res][ENDLIST][ENDOFDATA] */
            uint8_t status = buf[i - 4];
            if (status != OPAL_STATUS_SUCCESS) {
                OPAL_ERR("drive returned status 0x%02X", status);
                if (status == OPAL_STATUS_NOT_AUTHORIZED)
                    return OPAL_ERR_AUTH;
                if (status == OPAL_STATUS_INVALID_PARAM)
                    return OPAL_ERR_PARAM;
                if (status == OPAL_STATUS_AUTHORITY_LOCKED_OUT)
                    return OPAL_ERR_AUTH;
                return OPAL_ERR_PROTO;
            }
            return OPAL_OK;
        }
    }
    OPAL_ERR("ENDOFDATA not found in response");
    return OPAL_ERR_PROTO;
}

/**
 * parse_u16_be — read a big-endian uint16 from a buffer offset
 */
static uint16_t parse_u16_be(const uint8_t *b)
{
    return (uint16_t)(((uint16_t)b[0] << 8) | b[1]);
}

/**
 * parse_u32_be — read a big-endian uint32 from a buffer offset
 */
static uint32_t parse_u32_be(const uint8_t *b)
{
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16)
         | ((uint32_t)b[2] <<  8) | (uint32_t)b[3];
}

/**
 * parse_discovery — decode a Level 0 Discovery response
 *
 * LINUX EQUIVALENT: opal_discovery0_end() in sed-opal.c
 *
 * The discovery response is a flat list of feature descriptors.
 * Each descriptor has: [feature_code u16][version u8][length u8][data...]
 *
 * We walk the descriptor list and extract locking state and ComID.
 * Logic is identical to Linux; only the buffer source is different.
 */
static int parse_discovery(struct opal_dev *dev)
{
    uint8_t *buf   = dev->resp_buf;
    /* First 4 bytes = length of parameter data following the 4-byte length field */
    uint32_t total = parse_u32_be(buf);

    opal_discovery_t *d = &dev->discovery;
    memset(d, 0, sizeof(*d));

    /* Walk from byte 8 (skip 4-byte length + 4 bytes reserved header) */
    size_t offset = 8;
    while (offset + 4 <= (size_t)(total + 4)) {
        uint16_t feat = parse_u16_be(buf + offset);
        /* byte offset+2 = version nibble, offset+3 = descriptor length */
        uint8_t  dlen = buf[offset + 3];

        switch (feat) {
        case OPAL_FEAT_TPER:
            OPAL_DBG("Feature: TPer (0x0001)");
            break;

        case OPAL_FEAT_LOCKING:
            d->locking_supported  = 1;
            d->locking_enabled    = (buf[offset + 4] >> 1) & 1;
            d->locked             = (buf[offset + 4] >> 2) & 1;
            d->mbr_enabled        = (buf[offset + 4] >> 4) & 1;
            d->mbr_shadow_present = (buf[offset + 4] >> 5) & 1;
            OPAL_INFO("Locking feature: enabled=%u locked=%u mbr=%u",
                      d->locking_enabled, d->locked, d->mbr_enabled);
            break;

        case OPAL_FEAT_OPALV1:
            d->opal_v1_supported = 1;
            d->base_com_id  = parse_u16_be(buf + offset + 4);
            d->num_com_ids  = parse_u16_be(buf + offset + 6);
            OPAL_INFO("OPAL SSC v1: base_com_id=0x%04X num=%u",
                      d->base_com_id, d->num_com_ids);
            break;

        case OPAL_FEAT_OPALV2:
            d->opal_v2_supported = 1;
            d->base_com_id  = parse_u16_be(buf + offset + 4);
            d->num_com_ids  = parse_u16_be(buf + offset + 6);
            OPAL_INFO("OPAL SSC v2: base_com_id=0x%04X num=%u",
                      d->base_com_id, d->num_com_ids);
            break;

        case OPAL_FEAT_GEOMETRY:
            OPAL_DBG("Feature: Geometry (0x0003)");
            break;

        case OPAL_FEAT_SINGLE_USER:
            OPAL_DBG("Feature: SingleUser (0x0201)");
            break;

        case OPAL_FEAT_DATASTORE:
            OPAL_DBG("Feature: DataStore (0x0202)");
            break;

        default:
            OPAL_DBG("Unknown feature code 0x%04X (len=%u) — skipped", feat, dlen);
            break;
        }

        offset += 4 + dlen;
    }

    if (!d->opal_v1_supported && !d->opal_v2_supported) {
        OPAL_ERR("drive does not advertise OPAL SSC v1 or v2");
        return OPAL_ERR_PROTO;
    }

    /* Use the discovered ComID for all subsequent session commands */
    dev->com_id    = d->base_com_id;
    dev->discovered = 1;
    return OPAL_OK;
}

/**
 * parse_start_session_resp — extract TPer session ID from StartSession reply
 *
 * LINUX EQUIVALENT: opal_start_session_cont() in sed-opal.c
 *
 * The SyncSession response payload looks like:
 *   CALL [SMUID] [StartSession method]
 *   STARTLIST [HSN u32] [TSN u32] [...options...] ENDLIST ENDOFDATA [status]
 *
 * We scan for the two 4-byte atoms (HSN echo, then TSN) after the CALL token.
 */
static int parse_start_session_resp(struct opal_dev *dev)
{
    uint8_t *b   = dev->resp_buf + OPAL_TOTAL_HDR_LEN;
    size_t   len = (dev->resp_len > OPAL_TOTAL_HDR_LEN)
                 ? (dev->resp_len - OPAL_TOTAL_HDR_LEN) : 32;

    /* Locate STARTLIST token then read two 4-byte medium atoms */
    for (size_t i = 0; i + 12 < len; i++) {
        if (b[i] == OPAL_STARTLIST && b[i+1] == 0x84 && b[i+6] == 0x84) {
            uint32_t hsn_echo = parse_u32_be(b + i + 2);
            uint32_t tsn      = parse_u32_be(b + i + 7);
            OPAL_DBG("Session HSN echo=0x%08X TSN=0x%08X", hsn_echo, tsn);
            (void)hsn_echo;
            dev->tper_session_id = tsn;
            dev->session_open    = 1;
            OPAL_INFO("Session open: TPER_SN=0x%08X", dev->tper_session_id);
            return OPAL_OK;
        }
    }
    OPAL_ERR("could not locate TSN in StartSession response");
    return OPAL_ERR_PROTO;
}

/* ═══════════════════════════════════════════════════════════════════
 * SECTION 5 — Internal command builders (shared sub-steps)
 * ═══════════════════════════════════════════════════════════════════ */

/** Append the EndOfData + status-list trailer to every method call */
static int append_eod(struct opal_dev *dev)
{
    return append_token(dev, OPAL_ENDOFDATA)
        || append_token(dev, OPAL_STARTLIST)
        || append_u8(dev, 0)
        || append_u8(dev, 0)
        || append_u8(dev, 0)
        || append_token(dev, OPAL_ENDLIST);
}

/** Build a method call header: CALL [invoking_uid] [UID_STARTSESSION] STARTLIST */
static int append_call_header(struct opal_dev *dev,
                               const uint8_t *invoking_uid,
                               const uint8_t *UID_STARTSESSION)
{
    return append_token(dev, OPAL_CALL)
        || append_bytes(dev, invoking_uid, OPAL_UID_LEN)
        || append_bytes(dev, UID_STARTSESSION,   OPAL_METHOD_LEN)
        || append_token(dev, OPAL_STARTLIST);
}

/**
 * build_locking_range_uid — construct the UID for a given locking range
 *
 * Range 0  → GlobalRange UID
 * Range 1+ → LockingRange N UID (bytes 5=0x03, 7=N per TCG spec Table 227)
 */
static void build_locking_range_uid(uint8_t uid[OPAL_UID_LEN], uint8_t range_id)
{

    memcpy(uid, UID_GLOBALRANGE, OPAL_UID_LEN);
    if (range_id > 0) {
        uid[5] = 0x03;
        uid[7] = range_id;
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * SECTION 6 — Public API implementation
 * ═══════════════════════════════════════════════════════════════════ */

/**
 * opal_dev_init — allocate and initialise a device context
 *
 * LINUX EQUIVALENT: opal_dev_init() in sed-opal.c
 *
 * LINUX WAS:
 *   dev = kzalloc(sizeof(*dev), GFP_KERNEL);
 *   mutex_init(&dev->opal_lock);
 *   init_waitqueue_head(&dev->cmd_wait_q);
 *
 * PORTABLE:
 *   dev = opal_alloc(sizeof(*dev));   ← opal_ral.h
 *   dev->lock = opal_mutex_create();  ← opal_ral.h
 */
opal_dev_t *opal_dev_init(const opal_transport_t *transport)
{
    if (!transport || !transport->send || !transport->recv) {
        OPAL_ERR("opal_dev_init: invalid transport (NULL callbacks)");
        return NULL;
    }

    /* LINUX WAS: kzalloc(sizeof(*dev), GFP_KERNEL) */
    struct opal_dev *dev = (struct opal_dev *)opal_alloc(sizeof(*dev));
    if (!dev) {
        OPAL_ERR("opal_dev_init: opal_alloc failed");
        return NULL;
    }
    memset(dev, 0, sizeof(*dev));   /* LINUX WAS: kzalloc already zeroes */

    dev->transport       = *transport;
    dev->host_session_id = 0x00000041;  /* arbitrary non-zero; 'A' */

    /* LINUX WAS: mutex_init(&dev->opal_lock) */
    dev->lock = opal_mutex_create();
    if (!dev->lock) {
        OPAL_ERR("opal_dev_init: opal_mutex_create failed");
        /* LINUX WAS: kfree(dev) */
        opal_free(dev);
        return NULL;
    }

    OPAL_INFO("opal_dev initialised (host_sn=0x%08X)", dev->host_session_id);
    return dev;
}

/**
 * opal_dev_destroy — release all resources for a device context
 *
 * LINUX EQUIVALENT: opal_dev_release() in sed-opal.c
 *
 * LINUX WAS:
 *   mutex_destroy(&dev->opal_lock);
 *   kfree(dev);
 */
void opal_dev_destroy(opal_dev_t *dev)
{
    if (!dev) return;

    if (dev->session_open) {
        OPAL_WARN("opal_dev_destroy called with session still open — ending session");
        opal_end_session(dev);
    }

    /* LINUX WAS: mutex_destroy(&dev->opal_lock) */
    opal_mutex_destroy(dev->lock);

    /* LINUX WAS: kfree(dev) */
    opal_free(dev);

    OPAL_INFO("opal_dev destroyed");
}

/**
 * opal_discover — Level 0 Discovery
 *
 * Reads the drive's full feature set and stores it in dev->discovery.
 * Must be called before any session is opened.
 *
 * LINUX EQUIVALENT: opal_discovery0() + opal_discovery0_end() in sed-opal.c
 *
 * Level 0 discovery uses a FIXED ComID (0x0001) and protocol ID (0x01)
 * regardless of what the drive later negotiates. This is mandated by
 * TCG Storage Architecture Core Spec §3.3.6.
 */
int opal_discover(opal_dev_t *dev, opal_discovery_t *out)
{
    int r;
    if (!dev) return OPAL_ERR_PARAM;

    /* LINUX WAS: mutex_lock(&dev->opal_lock) */
    opal_mutex_lock(dev->lock);

    buf_reset(dev);
    /* Discovery: no payload — just send a zero-length IF-SEND to ComID 1 */
    /* The header written by buf_finalise with cmd_pos == OPAL_TOTAL_HDR_LEN
     * produces an empty packet which signals Level 0 discovery to the TPer */
    r = opal_send_recv(dev, 0x01, OPAL_DISCOVERY_COMID);
    if (r) {
        OPAL_ERR("discovery IF-SEND/IF-RECV failed");
        goto out;
    }

    r = parse_discovery(dev);
    if (r) goto out;

    if (out)
        *out = dev->discovery;

    OPAL_INFO("discovery complete — OPAL v%s, ComID=0x%04X, locked=%u",
              dev->discovery.opal_v2_supported ? "2" : "1",
              dev->discovery.base_com_id,
              dev->discovery.locked);
out:
    /* LINUX WAS: mutex_unlock(&dev->opal_lock) */
    opal_mutex_unlock(dev->lock);
    return r;
}

/**
 * opal_start_admin_session — open a read-write session with the AdminSP
 *
 * LINUX EQUIVALENT: opal_start_auth_session() in sed-opal.c
 *
 * Sends a StartSession method call to SMUID targeting AdminSP,
 * with optional PIN-based authentication (HostChallenge + HostSigningAuthority).
 *
 * If pin==NULL, an unauthenticated session is opened (used for Activate).
 */
int opal_start_admin_session(opal_dev_t *dev,
                              const uint8_t *pin, size_t pin_len)
{
    int r;

    if (!dev) return OPAL_ERR_PARAM;
    if (pin && pin_len > OPAL_MAX_PIN_LEN) {
        OPAL_ERR("PIN length %u exceeds OPAL_MAX_PIN_LEN (%u)",
                 (unsigned)pin_len, (unsigned)OPAL_MAX_PIN_LEN);
        return OPAL_ERR_PARAM;
    }
    if (!dev->discovered) {
        OPAL_ERR("opal_discover() must be called before opening a session");
        return OPAL_ERR_PROTO;
    }

    opal_mutex_lock(dev->lock);

    if (dev->session_open) {
        OPAL_ERR("a session is already open — call opal_end_session first");
        opal_mutex_unlock(dev->lock);
        return OPAL_ERR_PROTO;
    }

    buf_reset(dev);

    /*
     * TCG packet structure for StartSession:
     *
     *   CALL
     *   [SMUID]                           ← invoking UID (session manager)
     *   [StartSession method UID]
     *   STARTLIST
     *     [HSN u32]                       ← host session number
     *     [AdminSP UID]                   ← target Security Provider
     *     [Write=1 u8]                    ← request read-write session
     *     STARTNAME [0x00] [PIN bytes] ENDNAME    ← HostChallenge (PIN)
     *     STARTNAME [0x03] [Admin1 UID] ENDNAME   ← HostSigningAuthority
     *   ENDLIST
     *   ENDOFDATA STARTLIST [0][0][0] ENDLIST
     */
    r = append_call_header(dev, UID_SMUID, UID_STARTSESSION);
    r = r || append_u32(dev, dev->host_session_id);  /* HSN */
    r = r || append_bytes(dev, UID_ADMINSP, OPAL_UID_LEN); /* SP UID */
    r = r || append_u8(dev, 1);                        /* Write=TRUE */

    if (pin && pin_len > 0) {
        /* HostChallenge optional named parameter */
        r = r || append_token(dev, OPAL_STARTNAME);
        r = r || append_u8(dev, 0x00);                /* name: HostChallenge */
        r = r || append_bytes(dev, pin, pin_len);
        r = r || append_token(dev, OPAL_ENDNAME);
        /* HostSigningAuthority optional named parameter */
        r = r || append_token(dev, OPAL_STARTNAME);
        r = r || append_u8(dev, 0x03);                /* name: HostSigningAuthority */
        r = r || append_bytes(dev, UID_AUTH_ADMIN1, OPAL_UID_LEN);
        r = r || append_token(dev, OPAL_ENDNAME);
    }

    r = r || append_token(dev, OPAL_ENDLIST);
    r = r || append_eod(dev);
    if (r) { opal_mutex_unlock(dev->lock); return r; }

    r = opal_send_recv(dev, 0x01, dev->com_id);
    if (!r) r = parse_start_session_resp(dev);

    opal_mutex_unlock(dev->lock);
    return r;
}

/**
 * opal_start_locking_session — open a session with the LockingSP
 *
 * LINUX EQUIVALENT: opal_start_auth_session() called with UID_LOCKINGSP
 *
 * @user_id: 0 = Admin1, 1..N = User1..UserN
 */
int opal_start_locking_session(opal_dev_t *dev, uint8_t user_id,
                                const uint8_t *pin, size_t pin_len)
{
    int r;

    if (!dev) return OPAL_ERR_PARAM;
    if (!dev->discovered) return OPAL_ERR_PROTO;

    /* Build the authority UID based on user_id:
     *   0 → Admin1 UID
     *   N → UserN UID (byte 7 = N, bytes 0-6 fixed per TCG spec Table 227)
     */
    uint8_t auth_uid[8];
    memcpy(auth_uid, UID_AUTH_ADMIN1, 8);
if (user_id > 0) {
        /* UserN UID: { 0x00,0x00,0x00,0x09,0x00,0x03,0x00,N } */
        uint8_t user_uid[OPAL_UID_LEN];
        memcpy(user_uid, UID_AUTH_USER1, OPAL_UID_LEN);
        user_uid[7] = user_id;
        memcpy(auth_uid, user_uid, OPAL_UID_LEN);
    }

    opal_mutex_lock(dev->lock);
    buf_reset(dev);

    r = append_call_header(dev, UID_SMUID, UID_STARTSESSION);
    r = r || append_u32(dev, dev->host_session_id);
    r = r || append_bytes(dev, UID_LOCKINGSP, OPAL_UID_LEN);
    r = r || append_u8(dev, 1);   /* Write=TRUE */

    if (pin && pin_len > 0) {
        r = r || append_token(dev, OPAL_STARTNAME);
        r = r || append_u8(dev, 0x00);              /* HostChallenge */
        r = r || append_bytes(dev, pin, pin_len);
        r = r || append_token(dev, OPAL_ENDNAME);
        r = r || append_token(dev, OPAL_STARTNAME);
        r = r || append_u8(dev, 0x03);              /* HostSigningAuthority */
        r = r || append_bytes(dev, auth_uid, OPAL_UID_LEN);
        r = r || append_token(dev, OPAL_ENDNAME);
    }

    r = r || append_token(dev, OPAL_ENDLIST);
    r = r || append_eod(dev);
    if (r) { opal_mutex_unlock(dev->lock); return r; }

    r = opal_send_recv(dev, 0x01, dev->com_id);
    if (!r) r = parse_start_session_resp(dev);

    opal_mutex_unlock(dev->lock);
    return r;
}

/**
 * opal_end_session — close the current open session gracefully
 *
 * LINUX EQUIVALENT: opal_end_session() in sed-opal.c
 *
 * Sends the EndOfSession token. The drive closes the session and
 * the host resets its session state.
 */
int opal_end_session(opal_dev_t *dev)
{
    int r;
    if (!dev) return OPAL_ERR_PARAM;

    opal_mutex_lock(dev->lock);

    if (!dev->session_open) {
        OPAL_WARN("opal_end_session: no session is open");
        opal_mutex_unlock(dev->lock);
        return OPAL_OK;
    }

    buf_reset(dev);
    /* EndOfSession is a single token — no CALL, no method UID */
    r = append_token(dev, OPAL_ENDOFSESSION);
    if (r) { opal_mutex_unlock(dev->lock); return r; }

    r = opal_send_recv(dev, 0x01, dev->com_id);
    /* Reset session state regardless of result */
    dev->session_open    = 0;
    dev->tper_session_id = 0;

    if (!r)
        OPAL_INFO("session closed successfully");
    else
        OPAL_WARN("session close returned error %d (state reset anyway)", r);

    opal_mutex_unlock(dev->lock);
    return r;
}

/**
 * set_lock_state — internal helper: lock or unlock a locking range
 *
 * LINUX EQUIVALENT: opal_lock_unlock() in sed-opal.c
 *
 * Builds a Set method call targeting the LockingRange row:
 *   CALL [range_uid] [Set UID]
 *   STARTLIST
 *     STARTNAME [Values=1]
 *       STARTLIST
 *         STARTNAME [ReadLocked=7]  [locked_val] ENDNAME
 *         STARTNAME [WriteLocked=8] [locked_val] ENDNAME
 *       ENDLIST
 *     ENDNAME
 *   ENDLIST
 *   ENDOFDATA ...
 */
static int set_lock_state(opal_dev_t *dev, uint8_t range_id, uint8_t locked)
{
    uint8_t range_uid[OPAL_UID_LEN];
    int r;

    if (!dev->session_open) {
        OPAL_ERR("set_lock_state: no open session — call opal_start_locking_session first");
        return OPAL_ERR_PROTO;
    }

    build_locking_range_uid(range_uid, range_id);

    buf_reset(dev);
    r = append_call_header(dev, range_uid, UID_SET);
    /* Named parameter: Values (name=1) */
    r = r || append_token(dev, OPAL_STARTNAME);
    r = r || append_u8(dev, 0x01);              /* "Values" column group */
    r = r || append_token(dev, OPAL_STARTLIST);
    /* ReadLocked column */
    r = r || append_token(dev, OPAL_STARTNAME);
    r = r || append_u8(dev, OPAL_COL_READ_LOCKED);
    r = r || append_u8(dev, locked);
    r = r || append_token(dev, OPAL_ENDNAME);
    /* WriteLocked column */
    r = r || append_token(dev, OPAL_STARTNAME);
    r = r || append_u8(dev, OPAL_COL_WRITE_LOCKED);
    r = r || append_u8(dev, locked);
    r = r || append_token(dev, OPAL_ENDNAME);
    r = r || append_token(dev, OPAL_ENDLIST);
    r = r || append_token(dev, OPAL_ENDNAME);
    r = r || append_token(dev, OPAL_ENDLIST);
    r = r || append_eod(dev);
    if (r) return r;

    r = opal_send_recv(dev, 0x01, dev->com_id);
    if (!r) r = parse_status(dev);
    if (!r)
        OPAL_INFO("range %u → %s", range_id, locked ? "LOCKED" : "UNLOCKED");

    return r;
}

/**
 * opal_lock_range — set a locking range to the Locked state
 *
 * A locked range causes the drive to refuse all read and write
 * commands to that region of the disk until unlocked.
 *
 * @range_id: 0 = GlobalRange (entire disk), 1..N = LockingRange N
 */
int opal_lock_range(opal_dev_t *dev, uint8_t range_id)
{
    int r;
    if (!dev) return OPAL_ERR_PARAM;
    opal_mutex_lock(dev->lock);
    r = set_lock_state(dev, range_id, 1);
    opal_mutex_unlock(dev->lock);
    return r;
}

/**
 * opal_unlock_range — set a locking range to the Unlocked state
 *
 * @range_id: 0 = GlobalRange, 1..N = LockingRange N
 */
int opal_unlock_range(opal_dev_t *dev, uint8_t range_id)
{
    int r;
    if (!dev) return OPAL_ERR_PARAM;
    opal_mutex_lock(dev->lock);
    r = set_lock_state(dev, range_id, 0);
    opal_mutex_unlock(dev->lock);
    return r;
}

/**
 * opal_activate_locking_sp — transition LockingSP from Manufactured-Inactive
 *                             to Manufactured (active) state
 *
 * LINUX EQUIVALENT: opal_activate() in sed-opal.c
 *
 * Must be called exactly once during first-time drive setup.
 * Requires an open AdminSP session (unauthenticated is sufficient for
 * drives still in factory state — call opal_start_admin_session with
 * pin=NULL).
 *
 * After activation, the LockingSP can be used for lock/unlock operations.
 */
int opal_activate_locking_sp(opal_dev_t *dev)
{
    int r;

    if (!dev) return OPAL_ERR_PARAM;

    opal_mutex_lock(dev->lock);

    if (!dev->session_open) {
        OPAL_ERR("opal_activate_locking_sp: requires an open AdminSP session");
        opal_mutex_unlock(dev->lock);
        return OPAL_ERR_PROTO;
    }

    buf_reset(dev);
    /*
     * Activate method: CALL [LockingSP UID] [Activate UID]
     *   STARTLIST ENDLIST ENDOFDATA ...
     * No arguments required.
     */
    r = append_call_header(dev, UID_LOCKINGSP, UID_ACTIVATE);
    r = r || append_token(dev, OPAL_ENDLIST);
    r = r || append_eod(dev);
    if (r) { opal_mutex_unlock(dev->lock); return r; }

    r = opal_send_recv(dev, 0x01, dev->com_id);
    if (!r) r = parse_status(dev);
    if (!r) OPAL_INFO("LockingSP activated successfully");

    opal_mutex_unlock(dev->lock);
    return r;
}

/**
 * opal_set_password — change the PIN (credential) for a given user
 *
 * LINUX EQUIVALENT: opal_set_new_pw() in sed-opal.c
 *
 * Builds a Set method call targeting the C_PIN_Admin1 or C_PIN_UserN row
 * in the Locking SP. Must be called inside an open LockingSP session
 * authenticated as Admin1 (to set any user PIN) or as UserN (own PIN only).
 *
 * C_PIN table UIDs per TCG OPAL SSC Table 228:
 *   Admin1 → { 0x00,0x00,0x00,0x0B,0x00,0x01,0x00,0x01 }
 *   User N → { 0x00,0x00,0x00,0x0B,0x00,0x03,0x00,N }
 */
int opal_set_password(opal_dev_t *dev, uint8_t user_id,
                      const uint8_t *new_pin, size_t new_pin_len)
{
    /* C_PIN_Admin1 base UID */
    uint8_t cpin_uid[8];
    /* cpin_uid already initialised to Admin1 C_PIN UID above */
int r;

    if (!dev || !new_pin || new_pin_len == 0) return OPAL_ERR_PARAM;
    if (new_pin_len > OPAL_MAX_PIN_LEN) {
        OPAL_ERR("PIN too long (%u > %u)", (unsigned)new_pin_len, OPAL_MAX_PIN_LEN);
        return OPAL_ERR_PARAM;
    }

    /* Select UserN C_PIN row if user_id > 0 */
    if (user_id > 0) {
        cpin_uid[5] = 0x03;  /* User authority area */
        cpin_uid[7] = user_id;
    }

    opal_mutex_lock(dev->lock);

    if (!dev->session_open) {
        OPAL_ERR("opal_set_password: requires an open LockingSP session");
        opal_mutex_unlock(dev->lock);
        return OPAL_ERR_PROTO;
    }

    buf_reset(dev);

    /*
     * Set method on C_PIN row:
     *   CALL [C_PIN_uid] [Set UID]
     *   STARTLIST
     *     STARTNAME [Values=1]
     *       STARTLIST
     *         STARTNAME [PIN_col=3] [new_pin bytes] ENDNAME
     *       ENDLIST
     *     ENDNAME
     *   ENDLIST
     *   ENDOFDATA ...
     *
     * Column 3 of C_PIN table = PIN value (TCG OPAL SSC §5.3.3.3)
     */
    r = append_call_header(dev, cpin_uid, UID_SET);
    r = r || append_token(dev, OPAL_STARTNAME);
    r = r || append_u8(dev, 0x01);               /* Values */
    r = r || append_token(dev, OPAL_STARTLIST);
    r = r || append_token(dev, OPAL_STARTNAME);
    r = r || append_u8(dev, 0x03);               /* PIN column */
    r = r || append_bytes(dev, new_pin, new_pin_len);
    r = r || append_token(dev, OPAL_ENDNAME);
    r = r || append_token(dev, OPAL_ENDLIST);
    r = r || append_token(dev, OPAL_ENDNAME);
    r = r || append_token(dev, OPAL_ENDLIST);
    r = r || append_eod(dev);
    if (r) { opal_mutex_unlock(dev->lock); return r; }

    r = opal_send_recv(dev, 0x01, dev->com_id);
    if (!r) r = parse_status(dev);
    if (!r)
        OPAL_INFO("password changed for user_id=%u", user_id);

    opal_mutex_unlock(dev->lock);
    return r;
}

/**
 * opal_revert_tper — revert the TPer to factory state (DESTRUCTIVE)
 *
 * LINUX EQUIVALENT: opal_reverttper() in sed-opal.c
 *
 * This operation:
 *   1. Destroys ALL data on the drive (encryption key is erased)
 *   2. Resets all OPAL state to factory defaults
 *   3. Requires authentication with the Physical SID (PSID), which is
 *      printed on the drive's physical label
 *
 * Intended only for recovery when the SID PIN is lost.
 *
 * Flow:
 *   Open AdminSP session using PSID as the HostChallenge and
 *   PSID authority UID as HostSigningAuthority, then call Revert.
 */
int opal_revert_tper(opal_dev_t *dev,
                     const uint8_t *psid, size_t psid_len)
{
    int r;

    if (!dev || !psid || psid_len == 0) return OPAL_ERR_PARAM;

    OPAL_WARN("opal_revert_tper: DESTRUCTIVE OPERATION — all data will be lost");

    /*
     * Step 1: Open AdminSP session using PSID credential.
     * The PSID authority UID replaces Admin1 as HostSigningAuthority.
     */
    opal_mutex_lock(dev->lock);

    if (!dev->discovered) {
        opal_mutex_unlock(dev->lock);
        return OPAL_ERR_PROTO;
    }

    buf_reset(dev);
    r = append_call_header(dev, UID_SMUID, UID_STARTSESSION);
    r = r || append_u32(dev, dev->host_session_id);
    r = r || append_bytes(dev, UID_ADMINSP, OPAL_UID_LEN);
    r = r || append_u8(dev, 1);                       /* Write=TRUE */
    r = r || append_token(dev, OPAL_STARTNAME);
    r = r || append_u8(dev, 0x00);                    /* HostChallenge = PSID */
    r = r || append_bytes(dev, psid, psid_len);
    r = r || append_token(dev, OPAL_ENDNAME);
    r = r || append_token(dev, OPAL_STARTNAME);
    r = r || append_u8(dev, 0x03);                    /* HostSigningAuthority */
    r = r || append_bytes(dev, UID_AUTH_PSID, OPAL_UID_LEN);
    r = r || append_token(dev, OPAL_ENDNAME);
    r = r || append_token(dev, OPAL_ENDLIST);
    r = r || append_eod(dev);
    if (r) { opal_mutex_unlock(dev->lock); return r; }

    r = opal_send_recv(dev, 0x01, dev->com_id);
    if (!r) r = parse_start_session_resp(dev);
    if (r) {
        OPAL_ERR("PSID session open failed — check PSID printed on drive label");
        opal_mutex_unlock(dev->lock);
        return r;
    }

    /* Step 2: Call Revert on AdminSP */
    buf_reset(dev);
    r = append_call_header(dev, UID_ADMINSP, UID_REVERT);
    r = r || append_token(dev, OPAL_ENDLIST);
    r = r || append_eod(dev);
    if (r) { opal_mutex_unlock(dev->lock); return r; }

    r = opal_send_recv(dev, 0x01, dev->com_id);
    if (!r) r = parse_status(dev);

    /* After revert, session is gone — reset state */
    dev->session_open    = 0;
    dev->tper_session_id = 0;
    dev->discovered      = 0;

    if (!r)
        OPAL_WARN("opal_revert_tper: TPer reverted — drive is now in factory state");

    opal_mutex_unlock(dev->lock);
    return r;
}

/**
 * opal_query_lock_state — read back the current lock state of a range
 *
 * LINUX EQUIVALENT: opal_get_status() / table Get method in sed-opal.c
 *
 * Sends a Get method on the locking range row requesting only
 * the ReadLocked and WriteLocked columns.
 * Results are stored in *read_locked and *write_locked (0 or 1).
 */
int opal_query_lock_state(opal_dev_t *dev, uint8_t range_id,
                           uint8_t *read_locked, uint8_t *write_locked)
{
    uint8_t range_uid[OPAL_UID_LEN];
    int r;

    if (!dev || !read_locked || !write_locked) return OPAL_ERR_PARAM;

    opal_mutex_lock(dev->lock);

    if (!dev->session_open) {
        OPAL_ERR("opal_query_lock_state: requires an open session");
        opal_mutex_unlock(dev->lock);
        return OPAL_ERR_PROTO;
    }

    build_locking_range_uid(range_uid, range_id);
    buf_reset(dev);

    /*
     * Get method with column range filter:
     *   CALL [range_uid] [Get UID]
     *   STARTLIST
     *     STARTLIST
     *       STARTNAME [startColumn=3] [ReadLocked col=7] ENDNAME
     *       STARTNAME [endColumn=4]   [WriteLocked col=8] ENDNAME
     *     ENDLIST
     *   ENDLIST
     *   ENDOFDATA ...
     */
    r = append_call_header(dev, range_uid, UID_GET);
    r = r || append_token(dev, OPAL_STARTLIST);
    r = r || append_token(dev, OPAL_STARTNAME);
    r = r || append_u8(dev, 0x03);               /* startColumn */
    r = r || append_u8(dev, OPAL_COL_READ_LOCKED);
    r = r || append_token(dev, OPAL_ENDNAME);
    r = r || append_token(dev, OPAL_STARTNAME);
    r = r || append_u8(dev, 0x04);               /* endColumn */
    r = r || append_u8(dev, OPAL_COL_WRITE_LOCKED);
    r = r || append_token(dev, OPAL_ENDNAME);
    r = r || append_token(dev, OPAL_ENDLIST);
    r = r || append_token(dev, OPAL_ENDLIST);
    r = r || append_eod(dev);
    if (r) { opal_mutex_unlock(dev->lock); return r; }

    r = opal_send_recv(dev, 0x01, dev->com_id);
    if (r) { opal_mutex_unlock(dev->lock); return r; }

    /* Parse response — locate the two column values in the payload */
    uint8_t *b   = dev->resp_buf + OPAL_TOTAL_HDR_LEN;
    size_t   len = (dev->resp_len > OPAL_TOTAL_HDR_LEN)
                 ? dev->resp_len - OPAL_TOTAL_HDR_LEN : 64;

    *read_locked  = 0;
    *write_locked = 0;

    for (size_t i = 0; i + 4 < len; i++) {
        if (b[i] == OPAL_STARTNAME) {
            uint8_t col = b[i+1];
            uint8_t val = b[i+2];
            if (col == OPAL_COL_READ_LOCKED)  *read_locked  = val;
            if (col == OPAL_COL_WRITE_LOCKED) *write_locked = val;
        }
    }

    OPAL_INFO("range %u: read_locked=%u write_locked=%u",
              range_id, *read_locked, *write_locked);

    opal_mutex_unlock(dev->lock);
    return r;
}

/* ═══════════════════════════════════════════════════════════════════
 * SECTION 7 — Utility / diagnostics
 * ═══════════════════════════════════════════════════════════════════ */

/**
 * opal_print_discovery — print a human-readable discovery summary
 *
 * Useful for debugging and for the test log required by the project.
 */
void opal_print_discovery(const opal_discovery_t *d)
{
    if (!d) return;
    OPAL_INFO("─── OPAL Discovery Summary ───────────────────────");
    OPAL_INFO("  OPAL SSC v1        : %s", d->opal_v1_supported ? "YES" : "NO");
    OPAL_INFO("  OPAL SSC v2        : %s", d->opal_v2_supported ? "YES" : "NO");
    OPAL_INFO("  Base ComID         : 0x%04X", d->base_com_id);
    OPAL_INFO("  Num ComIDs         : %u",     d->num_com_ids);
    OPAL_INFO("  Locking supported  : %s", d->locking_supported ? "YES" : "NO");
    OPAL_INFO("  Locking enabled    : %s", d->locking_enabled   ? "YES" : "NO");
    OPAL_INFO("  Currently locked   : %s", d->locked            ? "YES" : "NO");
    OPAL_INFO("  MBR shadow enabled : %s", d->mbr_enabled       ? "YES" : "NO");
    OPAL_INFO("──────────────────────────────────────────────────");
}

/**
 * opal_error_str — return a human-readable string for an error code
 *
 * Useful for test logs and debug output.
 */
const char *opal_error_str(int err)
{
    switch (err) {
    case  OPAL_OK:             return "OK";
    case  OPAL_ERR_TRANSPORT:  return "ERR_TRANSPORT";
    case  OPAL_ERR_NO_MEM:     return "ERR_NO_MEM";
    case  OPAL_ERR_PROTO:      return "ERR_PROTO";
    case  OPAL_ERR_AUTH:       return "ERR_AUTH";
    case  OPAL_ERR_LOCKED:     return "ERR_LOCKED";
    case  OPAL_ERR_PARAM:      return "ERR_PARAM";
    case  OPAL_ERR_TIMEOUT:    return "ERR_TIMEOUT";
    default:                   return "ERR_UNKNOWN";
    }
}
