// simple test code to verify creation of tx command and parsing of rx response 

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "pms7003_esp/pms7003.cpp"

static bool assertEquals(int expected, int actual, const char *field)
{
    if (actual != expected) {
        fprintf(stderr, "Assertion failure: field '%s' expected %d, got %d\n", field, expected, actual);
        return false;
    }
    return true;
}

static bool test_rx(void)
{
    uint8_t frame[] = {
        // some garbage bytes
        0x00, 0xFF, 0x42,
        // header, length
        0x42, 0x4D, 0x00, 0x1C,
        // CF1
        0x00, 0x10, 0x00, 0x25, 0x01, 0x00,
        // amb
        0x00, 0x10, 0x00, 0x25, 0x01, 0x00,
        // raw
        0x00, 0x03, 0x00, 0x05, 0x00, 0x10,
        0x00, 0x25, 0x00, 0x50, 0x01, 0x00,
        // version, errorcode
        0x88, 0x99,
        // checksum
        0x02, 0xC6
    };

    // send frame data
    bool ok;
    uint8_t rxdata[32];
    PmsInit(rxdata, sizeof(rxdata));
    for (size_t i = 0; i < sizeof(frame); i++) {
        ok = PmsProcess(frame[i]);
    }
    if (!ok) {
        fprintf(stderr, "expected successful frame!");
        return false;
    }
    
    // parse
    pms_meas_t meas;
    PmsParse(&meas);
    ok = assertEquals(0x0010, meas.concPM1_0_CF1, "concPM1_0_CF1");
    ok = ok && assertEquals(0x0025, meas.concPM2_5_CF1, "concPM2_5_CF1");
    ok = ok && assertEquals(0x25, meas.concPM2_5_amb, "concPM2_5_amb");
    ok = ok && assertEquals(0x50, meas.rawGt5_0um, "rawGt5_0um");
    ok = ok && assertEquals(0x88, meas.version, "version");
    ok = ok && assertEquals(0x99, meas.errorCode, "errorCode");

    return ok;    
}

static bool test_tx(void)
{
    int res;
    uint8_t txbuf[16];

    // verify too small buffer
    res = PmsCreateCmd(txbuf, 6, 0xE0, 0);
    if (res > 0) {
        fprintf(stderr, "expected failure for too small buffer!");
        return false;
    }

    // verify valid message
    res = PmsCreateCmd(txbuf, sizeof(txbuf), 0xE1, 0x1234);
    assertEquals(7, res, "sizeof(txbuf)");

    const uint8_t expected[] = {0x42, 0x4d, 0xE1, 0x12, 0x34, 0x01, 0xB6};
    return (memcmp(expected, txbuf, sizeof(expected)) == 0);
}


int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    bool b;

    printf("test_rx ...");
    b = test_rx();
    printf("%s\n", b ? "PASS" : "FAIL");

    printf("test_tx ...");
    b = test_tx();
    printf("%s\n", b ? "PASS" : "FAIL");

    return 0;
}

