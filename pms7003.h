#include <stdbool.h>
#include <stdint.h>

// parsed measurement data
typedef struct {
    uint16_t concPM1_0_CF1;
    uint16_t concPM2_5_CF1;
    uint16_t concPM10_0_CF1;
    uint16_t concPM1_0_amb;
    uint16_t concPM2_5_amb;
    uint16_t concPM10_0_amb;
    uint16_t rawGt0_3um;
    uint16_t rawGt0_5um;
    uint16_t rawGt1_0um;
    uint16_t rawGt2_5um;
    uint16_t rawGt5_0um;
    uint16_t rawGt10_0um;
    uint8_t version;
    uint8_t errorCode;
} pms_meas_t;

void PmsInit(uint8_t *buf, int size);
bool PmsProcess(uint8_t b);
void PmsParse(pms_meas_t *meas);
int  PmsCreateCmd(uint8_t *buf, int size, uint8_t cmd, uint16_t data);

