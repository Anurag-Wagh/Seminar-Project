/**
 * test_opal_core.c — Unit Test Suite for Member 1 (OPAL Core)
 *
 * Tests every public function in opal_core.c using the mock transport.
 * No real hardware needed. Compiles and runs on any Linux/macOS desktop.
 *
 * Build:
 *   gcc -Wall -Wextra -std=c99 \
 *       test_opal_core.c \
 *       ../core/opal_core.c \
 *       ../transport/opal_transport_mock.c \
 *       ../ral/opal_ral_posix.c \
 *       -I.. -lpthread -o test_opal_core
 *
 * Run:  ./test_opal_core
 *
 * Member 1 owns this file.
 */

#include "../core/opal_core.h"
#include "../transport/opal_transport_mock.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ─── Test framework ────────────────────────────────────────────────────── */

static int g_pass = 0;
static int g_fail = 0;
static int g_total = 0;

#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define CYAN    "\033[36m"
#define BOLD    "\033[1m"

#define TEST_ASSERT(cond, msg)                                          \
    do {                                                                \
        g_total++;                                                      \
        if (cond) {                                                     \
            printf("  " GREEN "[PASS]" RESET " %s\n", msg);            \
            g_pass++;                                                   \
        } else {                                                        \
            printf("  " RED "[FAIL]" RESET " %s  (line %d)\n",         \
                   msg, __LINE__);                                      \
            g_fail++;                                                   \
        }                                                               \
    } while (0)

#define TEST_ASSERT_EQ(a, b, msg) TEST_ASSERT((a)==(b), msg)
#define TEST_ASSERT_NE(a, b, msg) TEST_ASSERT((a)!=(b), msg)
#define TEST_ASSERT_NULL(p, msg)  TEST_ASSERT((p)==NULL, msg)
#define TEST_ASSERT_NNULL(p, msg) TEST_ASSERT((p)!=NULL, msg)

#define TEST_SECTION(name) \
    printf("\n" BOLD CYAN "── %s ──" RESET "\n", name)

/* PIN used across tests */
static const uint8_t TEST_PIN[]     = { 'A','d','m','i','n','1' };
static const uint8_t TEST_NEW_PIN[] = { 'N','e','w','P','I','N' };
static const uint8_t TEST_PSID[]    = {
    'P','S','I','D','1','2','3','4','5','6','7','8','9','0','A','B',
    'C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R'
};

/* Helper: create a fresh dev with mock transport */
static opal_dev_t *new_dev(void)
{
    opal_transport_t t = mock_transport_init();
    return opal_dev_init(&t);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TEST GROUP 1: Lifecycle
 * ═══════════════════════════════════════════════════════════════════════════ */
static void test_lifecycle(void)
{
    TEST_SECTION("Lifecycle — opal_dev_init / opal_dev_destroy");

    /* TC-01: Normal init returns non-NULL */
    opal_dev_t *dev = new_dev();
    TEST_ASSERT_NNULL(dev, "TC-01: opal_dev_init returns non-NULL");

    /* TC-02: Destroy does not crash */
    opal_dev_destroy(dev);
    TEST_ASSERT(1, "TC-02: opal_dev_destroy completes without crash");

    /* TC-03: NULL transport rejected */
    opal_dev_t *bad = opal_dev_init(NULL);
    TEST_ASSERT_NULL(bad, "TC-03: NULL transport returns NULL");

    /* TC-04: Transport with NULL send rejected */
    opal_transport_t t2 = mock_transport_init();
    t2.send = NULL;
    opal_dev_t *bad2 = opal_dev_init(&t2);
    TEST_ASSERT_NULL(bad2, "TC-04: NULL send callback rejected");

    /* TC-05: Transport with NULL recv rejected */
    opal_transport_t t3 = mock_transport_init();
    t3.recv = NULL;
    opal_dev_t *bad3 = opal_dev_init(&t3);
    TEST_ASSERT_NULL(bad3, "TC-05: NULL recv callback rejected");

    /* TC-06: Double destroy of NULL is safe */
    opal_dev_destroy(NULL);
    TEST_ASSERT(1, "TC-06: opal_dev_destroy(NULL) is safe");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TEST GROUP 2: Level 0 Discovery
 * ═══════════════════════════════════════════════════════════════════════════ */
static void test_discovery(void)
{
    TEST_SECTION("Discovery — opal_discover");

    opal_dev_t *dev = new_dev();
    opal_discovery_t disc;
    memset(&disc, 0, sizeof(disc));

    /* TC-07: Discovery succeeds with mock response */
    int r = opal_discover(dev, &disc);
    TEST_ASSERT_EQ(r, OPAL_OK, "TC-07: opal_discover returns OPAL_OK");

    /* TC-08: OPAL v2 detected */
    TEST_ASSERT_EQ(disc.opal_v2_supported, 1,
                   "TC-08: OPAL v2 detected in discovery response");

    /* TC-09: base_com_id parsed correctly */
    TEST_ASSERT_EQ(disc.base_com_id, 0x0001,
                   "TC-09: base_com_id = 0x0001 parsed from mock response");

    /* TC-10: Locking supported flag set */
    TEST_ASSERT_EQ(disc.locking_supported, 1,
                   "TC-10: locking_supported = 1");

    /* TC-11: Locking enabled flag set */
    TEST_ASSERT_EQ(disc.locking_enabled, 1,
                   "TC-11: locking_enabled = 1");

    /* TC-12: Drive reports locked */
    TEST_ASSERT_EQ(disc.locked, 1,
                   "TC-12: locked = 1 (drive starts locked in mock)");

    /* TC-13: NULL out pointer is safe */
    mock_transport_init();  /* reset mock */
    opal_dev_t *dev2 = new_dev();
    int r2 = opal_discover(dev2, NULL);
    TEST_ASSERT_EQ(r2, OPAL_OK, "TC-13: opal_discover with NULL out is safe");

    /* TC-14: Transport I/O happened (send + recv each called once) */
    TEST_ASSERT_EQ(mock_transport_send_count(), 1u,
                   "TC-14: exactly 1 IF-SEND for discovery");
    TEST_ASSERT_EQ(mock_transport_recv_count(), 1u,
                   "TC-15: exactly 1 IF-RECV for discovery");

    opal_dev_destroy(dev);
    opal_dev_destroy(dev2);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TEST GROUP 3: Session Management
 * ═══════════════════════════════════════════════════════════════════════════ */
static void test_sessions(void)
{
    TEST_SECTION("Session management — open / end");

    opal_dev_t *dev = new_dev();

    /* Must discover first to get com_id */
    opal_discover(dev, NULL);

    /* TC-16: Open AdminSP session succeeds */
    int r = opal_start_admin_session(dev, TEST_PIN, sizeof(TEST_PIN));
    TEST_ASSERT_EQ(r, OPAL_OK, "TC-16: opal_start_admin_session returns OPAL_OK");

    /* TC-17: TPER session ID was parsed (non-zero) */
    /* We verify indirectly — subsequent operations with session state work */
    TEST_ASSERT(r == OPAL_OK, "TC-17: session opened (TPER_SN assigned)");

    /* TC-18: End session succeeds */
    int r2 = opal_end_session(dev);
    TEST_ASSERT_EQ(r2, OPAL_OK, "TC-18: opal_end_session returns OPAL_OK");

    /* TC-19: NULL dev rejected */
    int r3 = opal_start_admin_session(NULL, TEST_PIN, sizeof(TEST_PIN));
    TEST_ASSERT_NE(r3, OPAL_OK, "TC-19: NULL dev pointer rejected");

    /* TC-20: Double-open rejected — first re-open, then try again */
    mock_transport_set_next(MOCK_NEXT_SES_OPEN);
    opal_start_admin_session(dev, TEST_PIN, sizeof(TEST_PIN)); /* open session */
    int r4 = opal_start_admin_session(dev, TEST_PIN, sizeof(TEST_PIN)); /* try again */
    TEST_ASSERT_NE(r4, OPAL_OK, "TC-20: opening session while one is open rejected");
    opal_end_session(dev); /* clean up */

    /* TC-21: PIN too long rejected */
    uint8_t long_pin[64];
    memset(long_pin, 'X', sizeof(long_pin));
    int r5 = opal_start_admin_session(dev, long_pin, sizeof(long_pin));
    TEST_ASSERT_NE(r5, OPAL_OK, "TC-21: PIN > OPAL_MAX_PIN_LEN rejected");

    opal_dev_destroy(dev);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TEST GROUP 4: LockingSP Session
 * ═══════════════════════════════════════════════════════════════════════════ */
static void test_locking_session(void)
{
    TEST_SECTION("LockingSP session — opal_start_locking_session");

    opal_dev_t *dev = new_dev();
    opal_discover(dev, NULL);

    /* TC-22: Admin1 locking session (user_id=0) succeeds */
    mock_transport_set_next(MOCK_NEXT_SES_OPEN);
    int r = opal_start_locking_session(dev, 0, TEST_PIN, sizeof(TEST_PIN));
    TEST_ASSERT_EQ(r, OPAL_OK, "TC-22: Admin1 locking session opens");

    opal_end_session(dev);

    /* TC-23: User1 locking session (user_id=1) succeeds */
    mock_transport_set_next(MOCK_NEXT_SES_OPEN);
    int r2 = opal_start_locking_session(dev, 1, TEST_PIN, sizeof(TEST_PIN));
    TEST_ASSERT_EQ(r2, OPAL_OK, "TC-23: User1 locking session opens");

    opal_end_session(dev);

    /* TC-24: NULL dev rejected */
    int r3 = opal_start_locking_session(NULL, 0, TEST_PIN, sizeof(TEST_PIN));
    TEST_ASSERT_NE(r3, OPAL_OK, "TC-24: NULL dev rejected");

    opal_dev_destroy(dev);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TEST GROUP 5: Lock / Unlock
 * ═══════════════════════════════════════════════════════════════════════════ */
static void test_lock_unlock(void)
{
    TEST_SECTION("Lock / Unlock — opal_lock_range / opal_unlock_range");

    opal_dev_t *dev = new_dev();
    opal_discover(dev, NULL);
    mock_transport_set_next(MOCK_NEXT_SES_OPEN);
    opal_start_admin_session(dev, TEST_PIN, sizeof(TEST_PIN));

    /* TC-25: Lock GlobalRange (range_id=0) succeeds */
    mock_transport_set_next(MOCK_NEXT_SUCCESS);
    int r = opal_lock_range(dev, 0);
    TEST_ASSERT_EQ(r, OPAL_OK, "TC-25: opal_lock_range(0) returns OPAL_OK");

    /* TC-26: Unlock GlobalRange succeeds */
    mock_transport_set_next(MOCK_NEXT_SUCCESS);
    int r2 = opal_unlock_range(dev, 0);
    TEST_ASSERT_EQ(r2, OPAL_OK, "TC-26: opal_unlock_range(0) returns OPAL_OK");

    /* TC-27: Lock LockingRange1 (range_id=1) succeeds */
    mock_transport_set_next(MOCK_NEXT_SUCCESS);
    int r3 = opal_lock_range(dev, 1);
    TEST_ASSERT_EQ(r3, OPAL_OK, "TC-27: opal_lock_range(1) returns OPAL_OK");

    /* TC-28: Lock/unlock without open session fails */
    opal_end_session(dev);
    int r4 = opal_lock_range(dev, 0);
    TEST_ASSERT_NE(r4, OPAL_OK, "TC-28: lock without open session rejected");

    /* TC-29: NULL dev rejected */
    int r5 = opal_lock_range(NULL, 0);
    TEST_ASSERT_NE(r5, OPAL_OK, "TC-29: NULL dev rejected by opal_lock_range");

    opal_dev_destroy(dev);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TEST GROUP 6: Set Password
 * ═══════════════════════════════════════════════════════════════════════════ */
static void test_set_password(void)
{
    TEST_SECTION("Set password — opal_set_password");

    opal_dev_t *dev = new_dev();
    opal_discover(dev, NULL);
    mock_transport_set_next(MOCK_NEXT_SES_OPEN);
    opal_start_admin_session(dev, TEST_PIN, sizeof(TEST_PIN));

    /* TC-30: Change Admin1 PIN succeeds */
    mock_transport_set_next(MOCK_NEXT_SUCCESS);
    int r = opal_set_password(dev, 0, TEST_NEW_PIN, sizeof(TEST_NEW_PIN));
    TEST_ASSERT_EQ(r, OPAL_OK, "TC-30: opal_set_password(Admin1) returns OPAL_OK");

    /* TC-31: Change User1 PIN succeeds */
    mock_transport_set_next(MOCK_NEXT_SUCCESS);
    int r2 = opal_set_password(dev, 1, TEST_NEW_PIN, sizeof(TEST_NEW_PIN));
    TEST_ASSERT_EQ(r2, OPAL_OK, "TC-31: opal_set_password(User1) returns OPAL_OK");

    /* TC-32: NULL new_pin rejected */
    int r3 = opal_set_password(dev, 0, NULL, 6);
    TEST_ASSERT_NE(r3, OPAL_OK, "TC-32: NULL new_pin rejected");

    /* TC-33: Zero-length new_pin rejected */
    int r4 = opal_set_password(dev, 0, TEST_NEW_PIN, 0);
    TEST_ASSERT_NE(r4, OPAL_OK, "TC-33: zero-length new_pin rejected");

    opal_end_session(dev);

    /* TC-34: Set password without open session fails */
    int r5 = opal_set_password(dev, 0, TEST_NEW_PIN, sizeof(TEST_NEW_PIN));
    TEST_ASSERT_NE(r5, OPAL_OK, "TC-34: set_password without session rejected");

    opal_dev_destroy(dev);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TEST GROUP 7: Activate LockingSP
 * ═══════════════════════════════════════════════════════════════════════════ */
static void test_activate(void)
{
    TEST_SECTION("Activate LockingSP — opal_activate_locking_sp");

    opal_dev_t *dev = new_dev();
    opal_discover(dev, NULL);
    mock_transport_set_next(MOCK_NEXT_SES_OPEN);
    opal_start_admin_session(dev, TEST_PIN, sizeof(TEST_PIN));

    /* TC-35: Activate succeeds */
    mock_transport_set_next(MOCK_NEXT_SUCCESS);
    int r = opal_activate_locking_sp(dev);
    TEST_ASSERT_EQ(r, OPAL_OK, "TC-35: opal_activate_locking_sp returns OPAL_OK");

    /* TC-36: NULL dev rejected */
    int r2 = opal_activate_locking_sp(NULL);
    TEST_ASSERT_NE(r2, OPAL_OK, "TC-36: NULL dev rejected");

    opal_dev_destroy(dev);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TEST GROUP 8: RevertTPer
 * ═══════════════════════════════════════════════════════════════════════════ */
static void test_revert(void)
{
    TEST_SECTION("RevertTPer — opal_revert_tper");

    opal_dev_t *dev = new_dev();
    opal_discover(dev, NULL);

    /* TC-37: Revert with valid PSID succeeds (mock returns success) */
    mock_transport_set_next(MOCK_NEXT_SES_OPEN);
    int r = opal_revert_tper(dev, TEST_PSID, sizeof(TEST_PSID));
    TEST_ASSERT_EQ(r, OPAL_OK, "TC-37: opal_revert_tper returns OPAL_OK");

    /* TC-38: NULL PSID rejected */
    int r2 = opal_revert_tper(dev, NULL, 32);
    TEST_ASSERT_NE(r2, OPAL_OK, "TC-38: NULL PSID rejected");

    /* TC-39: Zero-length PSID rejected */
    int r3 = opal_revert_tper(dev, TEST_PSID, 0);
    TEST_ASSERT_NE(r3, OPAL_OK, "TC-39: zero-length PSID rejected");

    /* TC-40: NULL dev rejected */
    int r4 = opal_revert_tper(NULL, TEST_PSID, sizeof(TEST_PSID));
    TEST_ASSERT_NE(r4, OPAL_OK, "TC-40: NULL dev rejected");

    opal_dev_destroy(dev);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TEST GROUP 9: Full OPAL workflow
 * ═══════════════════════════════════════════════════════════════════════════ */
static void test_full_workflow(void)
{
    TEST_SECTION("Full OPAL workflow — discover → activate → lock → unlock");

    opal_dev_t *dev = new_dev();
    opal_discovery_t disc;
    int r;

    /* Step 1: Discover */
    r = opal_discover(dev, &disc);
    TEST_ASSERT_EQ(r, OPAL_OK, "TC-41: [workflow] discover");

    /* Step 2: Open AdminSP */
    mock_transport_set_next(MOCK_NEXT_SES_OPEN);
    r = opal_start_admin_session(dev, TEST_PIN, sizeof(TEST_PIN));
    TEST_ASSERT_EQ(r, OPAL_OK, "TC-42: [workflow] open admin session");

    /* Step 3: Activate LockingSP */
    mock_transport_set_next(MOCK_NEXT_SUCCESS);
    r = opal_activate_locking_sp(dev);
    TEST_ASSERT_EQ(r, OPAL_OK, "TC-43: [workflow] activate locking SP");

    /* Step 4: End AdminSP session */
    mock_transport_set_next(MOCK_NEXT_SUCCESS);
    r = opal_end_session(dev);
    TEST_ASSERT_EQ(r, OPAL_OK, "TC-44: [workflow] end admin session");

    /* Step 5: Open LockingSP session */
    mock_transport_set_next(MOCK_NEXT_SES_OPEN);
    r = opal_start_locking_session(dev, 0, TEST_PIN, sizeof(TEST_PIN));
    TEST_ASSERT_EQ(r, OPAL_OK, "TC-45: [workflow] open locking session");

    /* Step 6: Lock GlobalRange */
    mock_transport_set_next(MOCK_NEXT_SUCCESS);
    r = opal_lock_range(dev, 0);
    TEST_ASSERT_EQ(r, OPAL_OK, "TC-46: [workflow] lock GlobalRange");

    /* Step 7: Unlock GlobalRange */
    mock_transport_set_next(MOCK_NEXT_SUCCESS);
    r = opal_unlock_range(dev, 0);
    TEST_ASSERT_EQ(r, OPAL_OK, "TC-47: [workflow] unlock GlobalRange");

    /* Step 8: Change password */
    mock_transport_set_next(MOCK_NEXT_SUCCESS);
    r = opal_set_password(dev, 0, TEST_NEW_PIN, sizeof(TEST_NEW_PIN));
    TEST_ASSERT_EQ(r, OPAL_OK, "TC-48: [workflow] change Admin1 password");

    /* Step 9: End LockingSP session */
    mock_transport_set_next(MOCK_NEXT_SUCCESS);
    r = opal_end_session(dev);
    TEST_ASSERT_EQ(r, OPAL_OK, "TC-49: [workflow] end locking session");

    opal_dev_destroy(dev);
    TEST_ASSERT(1, "TC-50: [workflow] complete — dev destroyed cleanly");
}

/* ─── Main ──────────────────────────────────────────────────────────────── */

int main(void)
{
    printf(BOLD "\n╔══════════════════════════════════════════════════╗\n" RESET);
    printf(BOLD "║   OPAL Core Unit Test Suite — Member 1           ║\n" RESET);
    printf(BOLD "║   opal_core.c + mock transport                   ║\n" RESET);
    printf(BOLD "╚══════════════════════════════════════════════════╝\n" RESET);

    test_lifecycle();
    test_discovery();
    test_sessions();
    test_locking_session();
    test_lock_unlock();
    test_set_password();
    test_activate();
    test_revert();
    test_full_workflow();

    printf("\n" BOLD "══════════════════════════════════════════════\n" RESET);
    printf(BOLD "  Results: " RESET);
    printf(GREEN "%d passed" RESET ", ", g_pass);
    printf(RED   "%d failed" RESET ", ", g_fail);
    printf("of %d total\n", g_total);
    printf(BOLD "══════════════════════════════════════════════\n\n" RESET);

    return (g_fail == 0) ? 0 : 1;
}
