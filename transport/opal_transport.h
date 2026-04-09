/**
 * opal_transport.h — Storage/Transport Abstraction Interface
 *
 * Decouples the OPAL protocol core from the underlying storage
 * controller. To support a new hardware platform, implement a new
 * opal_transport_t and pass it to opal_dev_init().
 *
 * Member 3 owns this file and opal_transport_hw.c
 */

#ifndef OPAL_TRANSPORT_H
#define OPAL_TRANSPORT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum sizes for OPAL IF-SEND / IF-RECV buffers (TCG spec §3.3.6) */
#define OPAL_MAX_CMD_LEN   512
#define OPAL_MAX_RESP_LEN  512

/* Security protocol IDs used in TRUSTED SEND / TRUSTED RECEIVE */
#define OPAL_PROTO_ID_DISCOVERY  0x01   /* Level 0 discovery        */
#define OPAL_PROTO_ID_SESSION    0x01   /* Session commands (Locking)*/

/**
 * opal_send_fn - send an OPAL IF-SEND command to the drive
 *
 * @proto_id:   TCG security protocol identifier
 * @proto_sp:   protocol-specific field (ComID or SP identifier)
 * @buf:        payload buffer (OPAL packet)
 * @len:        payload length in bytes
 * @ctx:        opaque hardware context (set in opal_transport_t.ctx)
 *
 * Returns 0 on success, negative error code on failure.
 */
typedef int (*opal_send_fn)(uint8_t  proto_id,
                            uint16_t proto_sp,
                            const uint8_t *buf,
                            size_t   len,
                            void    *ctx);

/**
 * opal_recv_fn - receive an OPAL IF-RECV response from the drive
 *
 * @proto_id:   TCG security protocol identifier
 * @proto_sp:   protocol-specific field (ComID)
 * @buf:        output buffer (caller-allocated, at least OPAL_MAX_RESP_LEN)
 * @len:        size of output buffer
 * @ctx:        opaque hardware context
 *
 * Returns number of bytes received on success, negative on failure.
 */
typedef int (*opal_recv_fn)(uint8_t  proto_id,
                            uint16_t proto_sp,
                            uint8_t *buf,
                            size_t   len,
                            void    *ctx);

/**
 * opal_transport_t - pluggable transport backend
 *
 * Populate this struct with platform-specific callbacks and pass it
 * to opal_dev_init().  The OPAL core never calls hardware directly —
 * it always goes through this struct.
 */
typedef struct {
    opal_send_fn send;   /**< IF-SEND  implementation            */
    opal_recv_fn recv;   /**< IF-RECV  implementation            */
    void        *ctx;    /**< passed as-is to send/recv callbacks */
} opal_transport_t;

/**
 * opal_transport_validate - sanity-check a transport struct
 * Returns 0 if both callbacks are non-NULL, -1 otherwise.
 */
static inline int opal_transport_validate(const opal_transport_t *t)
{
    return (t && t->send && t->recv) ? 0 : -1;
}

#ifdef __cplusplus
}
#endif

#endif /* OPAL_TRANSPORT_H */
