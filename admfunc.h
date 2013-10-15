/* Copyright 2013 Bernhard R. Fischer, 2048R/5C5FFD47 <bf@abenteuerland.at>
 *
 * This file is part of Parseadm.
 *
 * Parseadm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 *
 * Parseadm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Parseadm. If not, see <http://www.gnu.org/licenses/>.
 */

/*! 
 *  
 *
 *  @author Bernhard R. Fischer
 */

#include <inttypes.h>


#define MAX_FAT_BLOCKLIST 240
#define FAT_SIZE 0x200

#define ADM_EPOCH ((time_t) 631062000L+3600)
#define ADM_LON_SCALE 11930463.0783    //<! tolerance 1.016E-5 - -6.299E-6
#define ADM_LAT_SCALE ADM_LON_SCALE
#define ADM_DEPTH_D 1067808470.8490566037
#define ADM_DEPTH_K 22137.4773584905
#define ADM_DEPTH(x) (((x) - ADM_DEPTH_D) / ADM_DEPTH_K)
#define ADM_DEPTH_NA 0x69045951


typedef struct adm_header
{
   char xor_byte;
   char null0[7];
   uint8_t ver_major;         //<! 0x008 map's major version
   uint8_t ver_minor;
   uint8_t upd_month;
   uint8_t upd_year;
   int16_t null1;
   int8_t mapsource;          //<! 0x00e 0: Garmin map, 1 Mapsource map
   char checksum;             //<! 0x00f
   char sig[7];               //<! 0x010 "DSKIMG\0"
   char b02_0;                //<! 0x017 always 2
   uint16_t sec0;
   uint16_t head0;
   uint16_t cyl0;
   char null2[18];
   char data[9];              //<! 0x030 unknown bytes, all 0 in IMG
   uint16_t creat_year;       //<! 0x039 creation year
   uint8_t creat_month;
   uint8_t creat_day;
   uint8_t creat_hour;
   uint8_t creat_min;
   uint8_t creat_sec;
   uint8_t fat_phys_block;    //<! 0x040 physical block # of FAT
   char ident[7];             //<! 0x041 "GARMIN\0"
   char null3;
   char map_desc[20];
   uint16_t head1;
   uint16_t sec1;
   uint8_t blocksize_e1;
   uint8_t blocksize_e2;
   uint16_t q;
   char map_name[31];
} __attribute__((packed)) adm_header_t;

typedef struct adm_partition
{
   char boot;
   uint8_t start_head;
   uint8_t start_sec;
   uint8_t start_cyl;
   uint8_t type;
   uint8_t end_head;
   uint8_t end_sec;
   uint8_t end_cyl;
   uint32_t rel_secs;
   uint32_t num_secs;
} __attribute__((packed)) adm_partition_t;

typedef struct adm_fat
{
   char subfile;           //<! 1 in real subfiles
   char sub_name[8];
   char sub_type[3];
   uint32_t sub_size;
   uint16_t next_fat;      /*<! not sure if this starts 1 byte later,
                             this is 0 in the first block and increments by 256
                             in each following block. */
   char y[14];
   uint16_t blocks[];
} __attribute__((packed)) adm_fat_t;

typedef struct adm_trk_header
{
   uint16_t hl;            //<! 0x000 common header length
   uint32_t len;           //<! 0x002 total length (including this header)
   int32_t a;
   char b;
   int32_t c;
   int16_t d;
   uint32_t len1;          //<! 0x011 len - 15
   int32_t e[6];           //<! 0x015 table of somethin
   int16_t f;              //<! 0x02d
   uint16_t name_len;      //<! 0x02f length of map name
   int32_t g[10];          //<! table of something
   char name[];            //<! 0x059 track name
} __attribute__((packed)) adm_trk_header_t;

typedef struct adm_trk_header2
{
   uint16_t num_tp;        //<! number of trackpoints
   int32_t x;
   int16_t y;
} __attribute__((packed)) adm_trk_header2_t;


typedef struct adm_track_point
{
   int32_t lat;         /*<! latitude and longitude linearly scaled by
                          ADM_LAT_SCALE and ADM_LON_SCALE */
   int32_t lon;
   int32_t timestamp;   /*<! timestamp starting at ADM_EPOCH after the epoch of
                          1.1.1970 */
   int32_t depth;       /*<! scaled depth, could be 2 int16_t fields as well
                          with the second one being the depth */
   char d;              //<! 0 or 1 at first point
   int32_t tempr;       //<! temperature
} __attribute__((packed)) adm_track_point_t; 

