/**
 * opal_transport_mock.h — Mock Transport for Unit Testing
 */
#ifndef OPAL_TRANSPORT_MOCK_H
#define OPAL_TRANSPORT_MOCK_H

#include "opal_transport.h"
#include <stdint.h>

/* Next-response selectors — plain ints, no enums to avoid naming conflicts */
#define MOCK_NEXT_DISCOVERY  0
#define MOCK_NEXT_SES_OPEN   1
#define MOCK_NEXT_SUCCESS    2
#define MOCK_NEXT_AUTH_FAIL  3

opal_transport_t mock_transport_init(void);
void             mock_transport_set_next(int op);
uint32_t         mock_transport_send_count(void);
uint32_t         mock_transport_recv_count(void);

#endif /* OPAL_TRANSPORT_MOCK_H */
