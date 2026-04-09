#include "opal_transport_hw.h"
#include "opal_ral.h"

static int opal_hw_send(uint8_t proto_id,
                         uint16_t proto_sp,
                         const uint8_t *buf,
                         size_t len,
                         void *ctx)
{
    (void)proto_id;
    (void)proto_sp;
    (void)buf;
    (void)len;
    (void)ctx;
    OPAL_WARN("opal_transport_hw_send stub called — hardware transport not implemented");
    return -1;
}

static int opal_hw_recv(uint8_t proto_id,
                         uint16_t proto_sp,
                         uint8_t *buf,
                         size_t len,
                         void *ctx)
{
    (void)proto_id;
    (void)proto_sp;
    (void)ctx;
    if (len > 0) {
        buf[0] = 0;
    }
    OPAL_WARN("opal_transport_hw_recv stub called — hardware transport not implemented");
    return -1;
}

int opal_transport_hw_init(opal_transport_t *t)
{
    if (!t) {
        return -1;
    }

    t->send = opal_hw_send;
    t->recv = opal_hw_recv;
    t->ctx  = NULL;
    OPAL_INFO("opal_transport_hw initialized with stub transport");
    return 0;
}

void opal_transport_hw_deinit(opal_transport_t *t)
{
    (void)t;
    OPAL_INFO("opal_transport_hw deinitialized");
}
