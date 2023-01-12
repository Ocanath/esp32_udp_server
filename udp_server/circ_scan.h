#ifndef CIRC_SCAN_H
#define CIRC_SCAN_H

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
    int write_idx;
    int read_idx;
    int read_size;
    //int size; //use sizeof bytes
    uint8_t full;

    int sb; //scanbyte
    int prev_maxsize;
}lg_fifo_t;


extern uint8_t scan_lg_fifo_fchk32(lg_fifo_t* pb, int expected_len, uint32_t * pbuf, int * len);
extern void add_circ_buffer_element(uint8_t new_data, lg_fifo_t* pb);

#endif