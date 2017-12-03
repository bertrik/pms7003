#include <stdbool.h>
#include <stdint.h>

#include "pms7003.h"

// parsing state
typedef enum {
    BEGIN1,
    BEGIN2,
    LENGTH,
    DATA,
    CHECK1,
    CHECK2
} EState;

typedef struct {
    EState  state;
    uint8_t *buf;
    int     size;
    int     idx, len;
    uint16_t chk, sum;
} TState;

static TState state;

void PmsInit(uint8_t *buf, int size)
{
    state.state = BEGIN1;
    state.buf = buf;
    state.size = size;
    state.idx = state.len = 0;
    state.chk = state.sum = 0;
}

bool PmsProcess(uint8_t b)
{
    switch (state.state) {
    // wait for BEGIN1 byte
    case BEGIN1:
        state.sum = b;
        if (b == 0x42) {
            state.state = BEGIN2;
        }
        break;
    // wait for BEGIN2 byte
    case BEGIN2:
        state.sum += b;
        if (b == 0x4D) {
            state.state = LENGTH;
        }
        break;
    // verify data length
    case LENGTH:
        state.sum += b;
        if (b <= state.size) {
            state.idx = 0;
            state.len = b;
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



