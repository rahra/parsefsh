/* Copyright 2013 Bernhard R. Fischer, 2048R/5C5FFD47 <bf@abenteuerland.at>
 *
 * This file is part of Parsefsh.
 *
 * Smrender is free software: you can redistribute it and/or modify
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
 */

#ifndef FSHFUNC_H
#define FSHFUNC_H

#include <stdint.h>

#define RL90_STR "RL90 FLASH FILE"
#define RFLOB_STR "RAYFLOB1"


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

// ellipsoid parameters for WGS84. e is calculated by init_ellipsoid()
#define WGS84 {6378137, 6356752.3142, 0}


/*** file structures of the ARCHIVE.FSH ***/

typedef struct fsh_file_header
{
   char rl90[16];    //!< constant terminated string "RL90 FLASH FILE"
   int32_t a;        //!< 1 or 8
   int16_t b;        //!< always 0
   int16_t c;        //!< always 1
   int16_t d;        //!< always 1
   int16_t e;        //!< always 1
   char rflob[8];    //!< constant unterminated string "RAYFLOB1"
   int16_t f;        //!< always 1
   int16_t g;        //!< always 1
   int16_t h;        //!< 0xfffc or 0xfff0
} __attribute__ ((packed)) fsh_file_header_t;

// total length 14 bytes
typedef struct fsh_track_point
{
   int32_t north, east; //!< prescaled (FSH_LAT_SCALE) northing and easting (ellipsoid Mercator)
   int16_t a;           //!< unknown
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

// total length 66 bytes
typedef struct fsh_track_meta
{
   char a;           //!< always 0x01
   int16_t cnt;      //!< number of track points
   int16_t _cnt;     //!< same as cnt
   int16_t b;        //!< unknown, always 0
   int16_t c;        //!< unknown
   int16_t d;        //!< unknown, always 0
   int32_t north_start; //!< Northing of first track point
   int32_t east_start;  //!< Easting of first track point
   int16_t e;        //!< unknown, same value as 'a' from first track point
   int16_t e1;       //!< unknown
   int16_t f;        //!< unknown, always 0
   int32_t north_end;   //!< Northing of last track point
   int32_t east_end;    //!< Easting of last track point
   int16_t g;        //!< unknown, same as 'a' from last track point
   int16_t g1;       //!< unknown
   int16_t h;        //!< unknown, always 0
   char i;           //!< unknown, 0, 1, or 5;
   char name[16];    //!< name of track, string not terminated
   int16_t j;        //!< unknown, probably flags, always 0x0100
   // at position 58, last 8 bytes are a guid
   uint64_t guid;
} __attribute__ ((packed)) fsh_track_meta_t;

// total length 14 bytes
typedef struct fsh_block_header
{
   uint16_t len;     //!< length of block in bytes excluding this header
   uint64_t guid;
   uint16_t type;    //!< type of block
   uint16_t unknown;
} __attribute__ ((packed)) fsh_block_header_t;

// route type 0x21
typedef struct fsh_route21_header
{
   int16_t a;        //!< unknown, always 0
   int16_t name_len; //!< length of name of route
   int16_t guid_cnt;
   uint16_t b;       //!< unknown
   char name[];      //!< unterminated name string of length name_len
} __attribute__ ((packed)) fsh_route21_header_t;

// route type 0x22
typedef struct fsh_route22_header
{
   int16_t name_len; //!< length of name of route
   int16_t guid_cnt;

} __attribute__ ((packed)) fsh_route22_header_t;

// waypoint length 56 bytes (without var. length name)
typedef struct fsh_wpt
{
   int64_t guid;
   int32_t lat, lon;    //!< latitude/longitude * 1E7
   int32_t north, east; //!< prescaled ellipsoid Mercator northing and easting
   char d[12];         //!< 12x \0
   char sym;           //!< probably symbol
   int16_t e;          //!< unknown, always -1
   int32_t f;          //!< unknown, always -1
   int32_t g;          //!< unknown
   uint16_t h;         //!< unknown
   char i;             //!< unknown, always 0
   int16_t name_len;   //!< length of name array
   int32_t j;          //!< unknown, always 0
   char name[];        //!< unterminated wpt name
} __attribute__ ((packed)) fsh_wpt_t;


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
   int16_t a;
   int16_t b;        //!< depth?
   int16_t c;        //!< always 0
   int16_t d;        //!< in the first element same value like b
   int16_t sym;      //!< seems to be the symbol
} __attribute__ ((packed));

struct fsh_hdr3
{
   int16_t wpt_cnt;  //!< number of waypoints
   int16_t a;        //!< always 0
} __attribute__ ((packed));


/*** memory structures used by parsefsh ***/

// fsh block
typedef struct fsh_block
{
   fsh_block_header_t hdr;
   void *data;
} __attribute__ ((packed)) fsh_block_t; 

// memory structure for keeping a track
typedef struct track
{
   fsh_block_header_t *bhdr;
   fsh_track_header_t *hdr;
   fsh_track_meta_t *mta;
   fsh_track_point_t *pt;

   int first_id, last_id;     //!< IDs, used for OSM output
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
   fsh_wpt_t *wpt;         //!< pointer to the first waypoint. Note, it does
                           //!< not increase linearly because fsh_wpt_t
                           //!< contains a variable length array if name_len length.

   int first_id, last_id;     //!< IDs, used for OSM output
} route21_t;

// structure to keep ellipsoid data
typedef struct ellipsoid
{
   double a;   //!< semi-major axis in m (equatorial)
   double b;   //!< semi-minor axis in m (polar)
   double e;   //!< eccentricity (this is derived from a and b, call init_ellipsoid())
} ellipsoid_t;

char *guid_to_string(uint64_t );
int fsh_read_file_header(int , fsh_file_header_t *);
fsh_block_t *fsh_block_read(int );
int fsh_track_decode(const fsh_block_t *, track_t **);
int fsh_route_decode(const fsh_block_t *, route21_t **);
void fsh_free_block_data(fsh_block_t *);

#endif

