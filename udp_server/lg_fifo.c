#include <stdint.h>

#define NUM_WORDS_FIFO   64 

typedef union u32_fmt_t
{
	uint32_t u32;
	int32_t i32;
	float f32;
	int16_t i16[sizeof(uint32_t) / sizeof(int16_t)];
	uint16_t ui16[sizeof(uint32_t) / sizeof(uint16_t)];
	int8_t i8[sizeof(uint32_t) / sizeof(int8_t)];
	uint8_t ui8[sizeof(uint32_t) / sizeof(uint8_t)];
}u32_fmt_t;

typedef struct lg_fifo_t
{
    uint8_t bytes[NUM_WORDS_FIFO * sizeof(u32_fmt_t)];
    int tail; //byte index of the tail of the buffer
    int sb;
    int prev_maxsize;
}lg_fifo_t;

const int nbytes_fifo = NUM_WORDS_FIFO*sizeof(u32_fmt_t);

/*
*/
uint8_t scan_lg_fifo_fchk32(lg_fifo_t* pb, int expected_len, void **pbuf, int * len)
{
    uint8_t rc = 0;
    int maxlen = pb->tail / sizeof(uint32_t);   //or divide by number of bytes in the checksum 
    if (pb->prev_maxsize != maxlen)
    {
        pb->sb = 0;
    }
    pb->prev_maxsize = maxlen;

    while (pb->sb < pb->tail)
    {
        int scanlen = ((pb->tail - pb->sb) / sizeof(uint32_t)) - 1;
        if (scanlen <= expected_len)    //nwords payload default 0
            return 0;
        else
        {
            uint32_t* scanbuf = (uint32_t*)(&pb->bytes[pb->sb]);
            uint32_t chk = fletchers_checksum32(scanbuf, expected_len);
            if (scanbuf[expected_len] == chk) //and potentially scanlen == expected_scanlen
            {
                //match found. 
                *pbuf = scanbuf;
                *len = scanlen;
                pb->tail = 0;  //everything before this came from an incomplete record and must be discarded
                pb->sb = 0;
                return 1;
            }
        }
        pb->sb++;
    }
    return rc;
}

uint8_t update_lg_fifo(uint8_t new_byte, lg_fifo_t* pb)
{
    if (pb->tail < nbytes_fifo)
    {
        pb->bytes[pb->tail] = new_byte;
        pb->tail++;
        return 0;
    }
    return 1;
}
