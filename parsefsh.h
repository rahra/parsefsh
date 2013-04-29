#ifndef PARSEFSH_H
#define PARSEFSH_H

#include <stdint.h>


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
typedef struct fsh_route
{
   int16_t a;        //!< unknown, always 0
   int16_t name_len; //!< length of name of route
   int16_t guid_cnt;
   uint16_t b;       //!< unknown

} __attribute__ ((packed)) fsh_route_t;

// memory structure for keeping a track
typedef struct track
{
   fsh_block_header_t *bhdr;
   fsh_track_header_t *hdr;
   fsh_track_meta_t *mta;
   fsh_track_point_t *pt;
   int first_id, last_id;
} track_t;

// structure to keep ellipsoid data
typedef struct ellipsoid
{
   double a;   //!< semi-major axis in m (equatorial)
   double b;   //!< semi-minor axis in m (polar)
   double e;   //!< eccentricity (this is derived from a and b, call init_ellipsoid())
} ellipsoid_t;

#endif

