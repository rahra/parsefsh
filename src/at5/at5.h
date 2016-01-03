#ifndef AT5_H
#define AT4_H

// little Endian format
//

struct at5_file_header
{
   int32_t at5_id[3];         //!< always 0x5aa55, 0xc, 0x0
   int32_t data_length;       //!< a little bit (43, 44) less than file length
   int32_t neg_file_length;   //!< file_length = -neg_file_length - 1
   int32_t hl;                //!< defines length of header (not yet know, how exactly)
   int16_t u1;                //!< a number, similar in several files
   int16_t u2;                //!< unknown
   int32_t u3[4];             //!< unknown
   char name_len;             //!< length of name
   char name[];               //!< name, not \0-terminated

} __attribute__((packed));

struct at5_h2
{
   char ds_len;
   char date_str[];           //!< \0-terminated
} __attribute__((packed));


typedef struct at5_addr_off
{
   int32_t addr;
   int32_t off;
} __attribute__((packed)) at5_addr_off_t;

struct at5_h3
{
   int32_t p0[2];
   int32_t p1[2];
   int32_t a0;                //!< always 0
   int32_t a1;                //!< always 8
   int32_t a2;                //!< always 8
   int32_t a4;                //!< some number, somehow related to hl;
   int32_t a5;                //!< always 9
   int32_t a6;                //!< a4 + 0x0a
   int32_t a7;                //!< always 3
   at5_addr_off_t ao0[8];
   int32_t a8;
   at5_addr_off_t ao1[3];
   int32_t a9;                //!< always 0xe0028003
   int32_t a10;               //!< always 0x00400101
} __attribute__((packed));

#endif

