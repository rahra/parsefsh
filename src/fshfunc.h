/* Copyright 2013 Bernhard R. Fischer, 2048R/5C5FFD47 <bf@abenteuerland.at>
 *
 * This file is part of Parsefsh.
 *
 * Parsefsh is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 *
 * Parsefsh is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Parsefsh. If not, see <http://www.gnu.org/licenses/>.
 */

/*! This file contains all data structurs used to parse the ARCHIVE.FSH.
 *
 *  @author Bernhard R. Fischer
 *
 *  ACKNOLEDGEMENTS
 *  Thanks to KHB for discovering depth, temperature, and timestamp fields in
 *  the waypoint structures!
 *  Thanks to Robbilard for the group22 header!
 */

#ifndef FSHFUNC_H
#define FSHFUNC_H

#include <stdint.h>

#define RL90_STR "RL90 FLASH FILE"
#define RFLOB_STR "RAYFLOB1"
#define FLOB_SIZE 0x10000

// maximum iterations to prevent from endless loops
#define MAX_IT 32
// iteration accuracy for reverse Mercator,
// approx 10cm in radians: 10cm / 100 / 1852 / 60 / 180 * M_PI
#define IT_ACCURACY 1.5E-8

// Northing in FSH is prescaled by this (empirically determined)
#define FSH_LAT_SCALE 107.1709342
// probably this value is more accurate, not sure.
//#define FSH_LAT_SCALE 107.1710725
// Easting is scaled by this
#define FSH_LON_SCALE ((double) 0x7fffffff)

#define TEMPR_NA ((uint16_t) 0xffff)
#define DEPTH_NA ((int32_t) -1)

// known FSH block types
#define FSH_BLK_WPT ((uint16_t) 0x0001)
#define FSH_BLK_TRK ((uint16_t) 0x000d)
#define FSH_BLK_MTA ((uint16_t) 0x000e)
#define FSH_BLK_RTE ((uint16_t) 0x0021)
#define FSH_BLK_GRP ((uint16_t) 0x0022)
#define FSH_BLK_ILL ((uint16_t) 0xffff)


/*** file structures of the ARCHIVE.FSH ***/

// total length 28 bytes
typedef struct fsh_file_header
{
   char rl90[16];    //!< constant terminated string "RL90 FLASH FILE"
   int16_t flobs;    //!< # of FLOBs, 0x10 (16) or 0x80 (128)
   int16_t a;        //!< always 0
   int16_t b;        //!< always 0
   int16_t c;        //!< always 1
   int16_t d;        //!< always 1
   int16_t e;        //!< always 1
} __attribute__ ((packed)) fsh_file_header_t;

// total length 14 bytes
typedef struct fsh_flob_header
{
   char rflob[8];    //!< constant unterminated string "RAYFLOB1"
   int16_t f;        //!< always 1
   int16_t g;        //!< always 1
   int16_t h;        //!< 0xfffe, 0xfffc, or 0xfff0
} __attribute__ ((packed)) fsh_flob_header_t;

// total length 14 bytes
typedef struct fsh_track_point
{
   int32_t north, east; //!< prescaled (FSH_LAT_SCALE) northing and easting (ellipsoid Mercator)
   uint16_t tempr;      //!< temperature in Kelvin * 100
   int16_t depth;       //!< depth in cm
   int16_t c;           //!< unknown, always 0
} __attribute__ ((packed)) fsh_track_point_t;

// type 0x0d blocks (list of track points) are prepended by this.
typedef struct fsh_track_header
{
   int32_t a;        //!< unknown, always 0
   int16_t cnt;      //!< number of track points
   int16_t b;        //!< unknown, always 0
} __attribute__ ((packed)) fsh_track_header_t;

// timestamp used in FSH data, length 6 bytes
typedef struct fsh_timestamp
{
   uint32_t timeofday;  //!< time of day in seconds
   uint16_t date;       //!< days since 1.1.1970
} __attribute__ ((packed)) fsh_timestamp_t;

// total length 58 + guid_cnt * 8 bytes
typedef struct fsh_track_meta
{
   char a;           //!< always 0x01
   int16_t cnt;      //!< number of track points
   int16_t _cnt;     //!< same as cnt
   int16_t b;        //!< unknown, always 0
   int32_t length;   //!< approx. track length in m
   int32_t north_start; //!< Northing of first track point
   int32_t east_start;  //!< Easting of first track point
   uint16_t tempr_start;//!< temperature of first track point
   int32_t depth_start; //!< depth of first track point
   int32_t north_end;   //!< Northing of last track point
   int32_t east_end;    //!< Easting of last track point
   uint16_t tempr_end;  //!< temperature last track point
   int32_t depth_end;   //!< depth of last track point
   char col;         /*!< track color: 0 - red, 1 - yellow, 2 - green, 3 -
                       blue, 4 - magenta, 5 - black */
   char name[16];    //!< name of track, string not terminated
   char j;           //!< unknown, always 0
   uint8_t guid_cnt; //!< nr of guids following this header
   uint64_t guid[];  //!< list of guids to this header belongs to
} __attribute__ ((packed)) fsh_track_meta_t;

// total length 14 bytes
typedef struct fsh_block_header
{
   uint16_t len;     //!< length of block in bytes excluding this header
   uint64_t guid;    //!< unique ID of block
   uint16_t type;    //!< type of block
   uint16_t unknown; //!< always 0x4000 ?
} __attribute__ ((packed)) fsh_block_header_t;

// route type 0x21
typedef struct fsh_route21_header
{
   int16_t a;        //!< unknown, always 0
   char name_len;    //!< length of name of route
   char cmt_len;     //!< length of comment
   int16_t guid_cnt; //!< number of guids following this header
   uint16_t b;       //!< unknown
   char txt_data[];  /*!< this is a combined string field of 'name' and
                       'comment' exactly like it is in the fsh_wpt_data_t (see
                       there). Use the macros NAME() and COMMENT() to retrieve
                       pointers to the strings. */
} __attribute__ ((packed)) fsh_route21_header_t;

// route type 0x22 (not seen yet in a file, I took this from fsh2gpx.py)
typedef struct fsh_route22_header
{
   int16_t name_len; //!< length of name of route
   int16_t guid_cnt;

} __attribute__ ((packed)) fsh_route22_header_t;

// common waypoint data, length 40 bytes + name_len + cmt_len
typedef struct fsh_wpt_data
{
   int32_t north, east; //!< prescaled ellipsoid Mercator northing and easting
   char d[12];         //!< 12x \0
   char sym;           //!< probably symbol
   
   uint16_t tempr;     //!< temperature in Kelvin * 100
   int32_t depth;      //!< depth in cm
   fsh_timestamp_t ts; //!< timestamp

   char i;             //!< unknown, always 0
   
   char name_len;      //!< length of name array
   char cmt_len;       //!< length of comment
   
   int32_t j;          //!< unknown, always 0
   char txt_data[];    /*!< this is a combined string field. First the name of
                         the waypoint of 'name_len' number of characters
                         directly followed by a comment of 'cmt_len'
                         characters. Thus, the comment starts at 'txt_data' +
                         'name_len'. */
#define NAME(x) ((x).txt_data)
#define COMMENT(x) ((x).txt_data + (x).name_len)
} __attribute__ ((packed)) fsh_wpt_data_t;

// waypoint as used int block type 0x22, 8 bytes + sizeof fsh_wpt_data_t
typedef struct fsh_wpt
{
   int32_t lat, lon;   //!< latitude/longitude * 1E7
   fsh_wpt_data_t wpd;
} __attribute__ ((packed)) fsh_wpt_t;

// route (0x21) waypoint is simply a GUID followed by the waypoint data, length
// is 8 bytes + sizeof fsh_wpt = 56 bytes + name_len + cmt_len
typedef struct fsh_route_wpt
{
  int64_t guid;    //!< the guid of the waypoint
  fsh_wpt_t wpt;   //!< rest of the waypoint data from above
} __attribute__ ((packed)) fsh_route_wpt_t;

struct fsh_hdr2
{
   int32_t lat0, lon0;  //!< lat/lon of first waypoint
   int32_t lat1, lon1;  //!< lat/lon of last waypoint
   int32_t a;
   //int16_t b;           //!< 0 or 1
   int16_t c;
   char d[24];
} __attribute__ ((packed));

struct fsh_pt
{
#if 1
   int16_t a;
   int16_t b;        //!< depth?
   int16_t c;        //!< always 0
   int16_t d;        //!< in the first element same value like b
   int16_t sym;      //!< seems to be the symbol
#else
   // looks also like this
   int16_t a;
   int32_t b;
   int32_t c;        //!< value is very similar to b
#endif

} __attribute__ ((packed));

struct fsh_hdr3
{
   int16_t wpt_cnt;  //!< number of waypoints
   int16_t a;        //!< always 0
} __attribute__ ((packed));

// group type 0x22
typedef struct fsh_group22_header
{
  int16_t name_len; //!< length of name of route
  int16_t guid_cnt; //!< number of GUIDs in the list following this header
  char name[];      //!< unterminated name string of length name_len
} __attribute__ ((packed)) fsh_group22_header_t;

// waypoint 0x01, length 8 bytes  + sizeof wpt_data_t
typedef struct fsh_wpt01
{
   int64_t guid;
   fsh_wpt_data_t wpd;
} __attribute__ ((packed)) fsh_wpt01_t;


/*** memory structures used by parsefsh ***/

// fsh block
typedef struct fsh_block
{
   fsh_block_header_t hdr;
   void *data;
} __attribute__ ((packed)) fsh_block_t; 

typedef struct track_segment
{
   fsh_block_header_t *bhdr;
   fsh_track_header_t *hdr;
   fsh_track_point_t *pt;
} track_segment_t;

// memory structure for keeping a track
typedef struct track
{
   fsh_block_header_t *bhdr;
   fsh_track_meta_t *mta;
   track_segment_t *tseg;

   int first_id, last_id;     //!< IDs, used for OSM output (FIXME: unclean impl.)
} track_t;

// mem struct for keeping a route
typedef struct route21
{
   fsh_block_header_t *bhdr;
   fsh_route21_header_t *hdr;
   int64_t *guid;
   //fsh_wpt_header_t *whdr;
   struct fsh_hdr2 *hdr2;
//   int pt_cnt;
   struct fsh_pt *pt;
   struct fsh_hdr3 *hdr3;
   fsh_route_wpt_t *wpt;   //!< pointer to the first waypoint. Note, it does
                           //!< not increase linearly because fsh_wpt_t
                           //!< contains a variable length array if name_len length.

   int first_id, last_id;     //!< IDs, used for OSM output
} route21_t;


char *guid_to_string(uint64_t );
int fsh_read_file_header(int , fsh_file_header_t *);
int fsh_read_flob_header(int , fsh_flob_header_t *);
fsh_block_t *fsh_block_read(int , fsh_block_t *);
int fsh_track_decode(const fsh_block_t *, track_t **);
int fsh_route_decode(const fsh_block_t *, route21_t **);
void fsh_free_block_data(fsh_block_t *);
int fsh_timetostr(const fsh_timestamp_t *, char *, int );

#endif

