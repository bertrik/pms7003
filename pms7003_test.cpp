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
    uint8_t data[32];
    pms_meas_t meas;

    PmsInit(data, sizeof(data));

    // send some garbage bytes
    PmsProcess(0x00);
    PmsProcess(0xFF);
    PmsProcess(0x42);

    // send the actual frame
    PmsProcess(0x42); PmsProcess(0x4D);
    PmsProcess(0x00); PmsProcess(0x1C);
    
    PmsProcess(0x00); PmsProcess(0x10);
    PmsProcess(0x00); PmsProcess(0x25);
    PmsProcess(0x01); PmsProcess(0x00);
    
    PmsProcess(0x00); PmsProcess(0x10);
    PmsProcess(0x00); PmsProcess(0x25);
    PmsProcess(0x01); PmsProcess(0x00);
    
    PmsProcess(0x00); PmsProcess(0x03);
    PmsProcess(0x00); PmsProcess(0x05);
    PmsProcess(0x00); PmsProcess(0x10);
    PmsProcess(0x00); PmsProcess(0x25);
    PmsProcess(0x00); PmsProcess(0x50);
    PmsProcess(0x01); PmsProcess(0x00);
    
    PmsProcess(0x88);
    PmsProcess(0x99);
    
    // checksum
    PmsProcess(0x02);
    bool ok = PmsProcess(0xC6);
    if (!ok) {
        fprintf(stderr, "expected successful frame!");
        return false;
    }
    
    // parse
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

