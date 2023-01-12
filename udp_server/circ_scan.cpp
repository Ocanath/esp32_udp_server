#include "circ_scan.h"
#include "checksum.h"

//const int nbytes_fifo = NUM_WORDS_FIFO*sizeof(u32_fmt_t);

/*
*/
uint8_t scan_lg_fifo_fchk32(lg_fifo_t* pb, int expected_len, uint32_t * pbuf, int * len)
{
    uint8_t rc = 0;
    int maxlen = pb->read_size / sizeof(uint32_t);   //or divide by number of bytes in the checksum 
    if (pb->prev_maxsize != maxlen)
    {
        pb->sb = 0;
    }
    pb->prev_maxsize = maxlen;

    if (pb->full != 0)
        pb->sb = 0; //scan from start every time in the case of a full buffer

    while (pb->sb < pb->read_size)
    {
        int scanlen = ((pb->read_size - pb->sb) / sizeof(uint32_t)) - 1;
        if (scanlen < expected_len)    //nwords payload default 0  
            return 0;
        else
        {
            int bidx = (pb->sb + pb->read_idx) % sizeof(pb->bytes); //establish scan start

            //copy circular buffer contents into the result container for scanning & reporting. add 1 to expected len to include checksum into the copy
            uint8_t* p8 = (uint8_t*)pbuf;
            int cpy_idx = 0;
            for (int i = 0; i < (expected_len+1); i++)
            {
                for (int j = 0; j < sizeof(uint32_t); j++)
                {
                    p8[cpy_idx++] = pb->bytes[bidx];
                    //bidx = (bidx + 1) % sizeof(pb->bytes);
                    bidx = bidx + 1;
                    if (bidx >= sizeof(pb->bytes))
                        bidx = 0;
                }
            }

            //uint32_t* scanbuf = (uint32_t*)(&pb->bytes[bidx]);
            uint32_t chk = fletchers_checksum32(pbuf, expected_len);
            if (pbuf[expected_len] == chk) //and potentially scanlen == expected_scanlen
            {
                //match found. 
                *len = scanlen;
                pb->read_idx = 0;
                pb->read_size = 0;
                pb->write_idx = 0;
                pb->full = 0;

                pb->sb = 0;
                return 1;
            }
        }
        pb->sb++;
    }
    return rc;
}

/*could generalize data32 to a void * and entry size, but imma keep it as is for now*/
void add_circ_buffer_element(uint8_t new_data, lg_fifo_t* pb)
{
    //memcpy(&cb->buf[cb->write_idx], new_data, data32_num_words * sizeof(u32_fmt_t));
    pb->bytes[pb->write_idx] = new_data;
    pb->write_idx = pb->write_idx + 1;

    if (pb->full == 0)
    {
        pb->read_idx = 0;
        pb->read_size++;
    }
    else
    {
        pb->read_idx = pb->write_idx;
    }

    if (pb->write_idx >= sizeof(pb->bytes))
    {
        pb->write_idx = 0;
        pb->full = 1;
    }
}
