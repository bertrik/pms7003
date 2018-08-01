#include <stdbool.h>
#include <stdint.h>

#include "pms7003.h"

// magic header bytes (actually ASCII 'B' and 'M')
#define MAGIC1 0x42
#define MAGIC2 0x4D

// parsing state
typedef enum {
    BEGIN1,
    BEGIN2,
    LENGTH1,
    LENGTH2,
    DATA,
    CHECK1,
    CHECK2
} EState;

typedef struct {
    EState  state;
    uint8_t buf[32];
    int     size;
    int     idx, len;
    uint16_t chk, sum;
} TState;

static TState state;

/**
    Initializes the measurement data state machine.
 */
void PmsInit(void)
{
    state.state = BEGIN1;
    state.size = sizeof(state.buf);
    state.idx = state.len = 0;
    state.chk = state.sum = 0;
}

/**
    Processes one byte in the measurement data state machine.
    @param[in] b the byte
    @return true if a full message was received
 */
bool PmsProcess(uint8_t b)
{
    switch (state.state) {
    // wait for BEGIN1 byte
    case BEGIN1:
        state.sum = b;
        if (b == MAGIC1) {
            state.state = BEGIN2;
        }
        break;
    // wait for BEGIN2 byte
    case BEGIN2:
        state.sum += b;
        if (b == MAGIC2) {
            state.state = LENGTH1;
        } else {
            state.state = BEGIN1;
            // retry
            return PmsProcess(b);
        }
        break;
    // verify data length
    case LENGTH1:
        state.sum += b;
        state.len = b << 8;
        state.state = LENGTH2;
        break;
    case LENGTH2:
        state.sum += b;
        state.len += b;
        state.len -= 2;     // exclude checksum bytes
        if (state.len <= state.size) {
            state.idx = 0;
            state.state = DATA;
        } else {
            // bogus length
            state.state = BEGIN1;
        }
        break;
    // store data
    case DATA:
        state.sum += b;
        if (state.idx < state.len) {
            state.buf[state.idx++] = b;
        }
        if (state.idx == state.len) {
            state.state = CHECK1;
        }
        break;
    // store checksum
    case CHECK1:
        state.chk = b << 8;
        state.state = CHECK2;
        break;
    // verify checksum
    case CHECK2:
        state.chk += b;
        state.state = BEGIN1;
        return (state.chk == state.sum);
    default:
        state.state = BEGIN1;
        break;
    }
    return false;
}

static uint16_t get(uint8_t *buf, int idx)
{
    uint16_t data;
    data = buf[idx] << 8;
    data += buf[idx + 1];
    return data;
}

/**
    Get length of packet's data frame.
    @return data frame length of packet.
 */
int PmsGetDataLen()
{
    return state.len;
}

/**
    Parses a complete measurement data frame into a structure.
    @param[out] meas the parsed measurement data
 */
void PmsParse(pms_meas_t *meas)
{
    meas->concPM1_0_CF1 = get(state.buf, 0);
    meas->concPM2_5_CF1 = get(state.buf, 2);
    meas->concPM10_0_CF1 = get(state.buf, 4);
    meas->concPM1_0_amb = get(state.buf, 6);
    meas->concPM2_5_amb = get(state.buf, 8);
    meas->concPM10_0_amb = get(state.buf, 10);
    meas->rawGt0_3um = get(state.buf, 12);
    meas->rawGt0_5um = get(state.buf, 14);
    meas->rawGt1_0um = get(state.buf, 16);
    meas->rawGt2_5um = get(state.buf, 18);
    meas->rawGt5_0um = get(state.buf, 20);
    meas->rawGt10_0um = get(state.buf, 22);
    meas->version = state.buf[24];
    meas->errorCode  = state.buf[25];
}

/**
    Parses a 2-byte response frame.
    For command PMS_CMD_AUTO_MANUAL (0xe1) returns command (1b) and data (1b) the same as in query frame.
    @return 16bit response.
 */
uint16_t PmsParse16()
{
    if(state.len != 2)
        return 0;

    return get(state.buf, 0);
}

/**
    Creates a command buffer to send.
    @param[in] buf the command buffer
    @param[in] size the size of the command buffer, should be at least 7 bytes
    @param[in] cmd the command byte
    @param[in] data the data field
    @return the length of the command buffer, or 0 if the size was too small
*/
int PmsCreateCmd(uint8_t *buf, int size, uint8_t cmd, uint16_t data)
{
    if (size < 7) {
        return 0;
    }

    int idx = 0;
    buf[idx++] = MAGIC1;
    buf[idx++] = MAGIC2;
    buf[idx++] = cmd;
    buf[idx++] = (data >> 8) & 0xFF;
    buf[idx++] = (data >> 0) & 0xFF;
    int sum = 0;
    for (int i = 0; i < idx; i++) {
        sum += buf[i];
    }
    buf[idx++] = (sum  >> 8) & 0xFF;
    buf[idx++] = (sum  >> 0) & 0xFF;
    return idx;
}

