#ifndef PARSEFSH_H
#define PARSEFSH_H

#include <stdint.h>

#define RL90_STR "RL90 FLASH FILE"
#define RFLOB_STR "RAYFLOB1"


// maximum iterations to prevent from endless loops
#define MAX_IT 32
// iteration accuracy for reverse Mercator,
// approx 10cm in radians: 10cm / 100 / 1852 / 60 / 180 * M_PI
#define IT_ACCURACY 1.5E-8

// Northing in FSH is prescaled by this
#define FSH_LAT_SCALE 107.1709342
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
   int32_t lat, lon;
   int16_t a;        //!< unknown
   int16_t depth;    //!< depth in cm
   int16_t c;        //!< unknown, always 0
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
   int32_t lat_start;
   int32_t lon_start;
   int32_t e;        //!< unknown
   int16_t f;        //!< unknown, always 0
   int32_t lat_end;
   int32_t lon_end;
   int32_t g;        //!< unknown
   int16_t h;        //!< unknown, always 0
   char i;           //!< unknown, always 5;
   char name1[8];
   char name2[8];    //!< not sure if name2 is slack space of name1
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

// fsh block
typedef struct fsh_block
{
   fsh_block_header_t hdr;
   void *data;
} __attribute__ ((packed)) fsh_block_t; 

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

/*// header before waypoints in route after guid list
typedef struct fsh_wpt_header
{
   uint64_t time1;
   uint64_t time2;
   uint32_t weird;      //!< according to Emmons
   char a[26];

} __attribute__ ((packed)) fsh_wpt_header_t;
*/

// waypoint length 56 bytes (without var. length name)
typedef struct fsh_wpt
{
   int64_t guid;
   int32_t lat, lon;
   uint64_t timestamp;
   char d[12];          //!< 12x \0
   char sym;            //!< probably symbol
   int16_t e;          //!< unknown, always -1
   int32_t f;          //!< unknown, always -1
   int32_t g;          //!< unknown
   uint16_t h;         //!< unknown
   char i;             //!< unknown, always 0
   int16_t name_len;    //!< length of name array
   int32_t j;           //!< unknown, always 0
   char name[];         //!< unterminated wpt name
} __attribute__ ((packed)) fsh_wpt_t;


struct fsh_hdr2
{
   int32_t lat, lon;
   int64_t timestamp;
   int16_t a;
   int16_t b;           //!< 0 or 1
   int16_t c;
   char d[24];
} __attribute__ ((packed));

struct fsh_pt
{
   int16_t a;
   int16_t b;
   int16_t c;        //!< always 0
   int16_t d;        //!< in the first element same value like b
   int16_t e;        //!< mostly 1 or 2
} __attribute__ ((packed));

struct fsh_hdr3
{
   int16_t wpt_cnt;  //!< number of waypoints
   int16_t a;        //!< always 0
} __attribute__ ((packed));


/*** memory structures used by parsefsh ***/

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
} route21_t;

// structure to keep ellipsoid data
typedef struct ellipsoid
{
   double a;   //!< semi-major axis in m (equatorial)
   double b;   //!< semi-minor axis in m (polar)
   double e;   //!< eccentricity (this is derived from a and b, call init_ellipsoid())
} ellipsoid_t;

#endif

