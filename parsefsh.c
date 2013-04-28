#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <math.h>
#include <time.h>


// maximum iterations to prevent from endless loops
#define MAX_IT 32
// accuracy approx. 10 cm
#define IT_ACCURACY 1.5E-8

#define FSH_LAT_SCALE 107.1709342
#define FSH_LON_SCALE ((double) 0x7fffffff)


// structure to keep ellipsoid data
struct ellipsoid
{
   double a;   //!< semi-major axis in m (equatorial)
   double b;   //!< semi-minor axis in m (polar)
   double e;   //!< eccentricity (this is derived from a and b, call init_ellipsoid())
};

// total length 14 bytes
struct fsh_track_point
{
   int32_t lat, lon;
   int16_t a;        //!< unknown
   int16_t depth;    //!< depth in cm
   int16_t c;        //!< unknown, always 0
} __attribute__ ((packed));

// type 0x0d blocks (list of track points) are prepended by this.
struct fsh_track_header
{
   int32_t a;        //!< unknown, always 0
   int16_t cnt;      //!< number of track points
   int16_t b;        //!< unknown, always 0
} __attribute__ ((packed));

// total length 66 bytes
struct fsh_track_meta
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
} __attribute__ ((packed));

// total length 14 bytes
struct fsh_block_header
{
   uint16_t len;     //!< length of block in bytes excluding this header
   uint64_t guid;
   uint16_t type;    //!< type of block
   uint16_t unknown;
} __attribute__ ((packed));

// fsh block
struct fsh_block
{
   struct fsh_block_header hdr;
   void *data;
} __attribute__ ((packed));


// memory structure for keeping a track
struct fsh_track
{
   struct fsh_block_header *bhdr;
   struct fsh_track_header *hdr;
   struct fsh_track_meta *mta;
   struct fsh_track_point *pt;
};


const int RL90_HEADER_LEN = 28;
const int RFLOB_HEADER_LEN = 14;
const char *RL90  = "RL90 FLASH FILE";
const char *RFLOB = "RAYFLOB1";

static const char hex_[] = "0123456789abcdef";


static void init_ellipsoid(struct ellipsoid *el)
{
   el->e = sqrt(1 - pow(el-> b / el-> a, 2));
}


static double phi_rev_merc(const struct ellipsoid *el, double N, double phi0)
{
   double esin = el->e * sin(phi0);
   return M_PI_2 - 2.0 * atan(exp(-N / el->a) * pow((1 - esin) / (1 + esin), el->e / 2));
}


static double phi_iterate_merc(const struct ellipsoid *el, double N)
{
   double phi, phi0;
   int i;

   for (phi = 0, phi0 = M_PI, i = 0; fabs(phi - phi0) > IT_ACCURACY && i < MAX_IT; i++)
   {
      phi0 = phi;
      phi = phi_rev_merc(el, N, phi0);
   }

   return phi;
}


static void hexdump(void *buf, int len)
{
   int i;

   for (i = 0; i < len; i++)
      printf("%c%c ", hex_[(((char*) buf)[i] >> 4) & 15], hex_[((char*) buf)[i] & 15]);
   printf("\n");
}


static int ptime(long t)
{
   printf(" %s", ctime(&t));
   return 0;
}

int read_rl90(int fd)
{
   char buf[RL90_HEADER_LEN];

   if (read(fd, buf, sizeof(buf)) == -1)
      perror("read"), exit(EXIT_FAILURE);

   if (memcmp(buf, RL90, strlen(RL90)))
      fprintf(stderr, "no RL90 header\n"),
         exit(EXIT_FAILURE);

   fprintf(stderr, "# rl90 byte: %02x\n", buf[16] & 0xff);

   return 0;
}


int read_rayflob(int fd)
{
   char buf[RFLOB_HEADER_LEN];

   if (read(fd, buf, sizeof(buf)) == -1)
      perror("read"), exit(EXIT_FAILURE);

   if (memcmp(buf, RFLOB, strlen(RFLOB)))
      fprintf(stderr, "no RFLOB header\n"),
         exit(EXIT_FAILURE);

   return 0;
}


static char *guid_to_string(uint64_t guid)
{
   static char buf[32];

   snprintf(buf, sizeof(buf),  "%ld-%ld-%ld-%ld",
         guid >> 48, (guid >> 32) & 0xffff, (guid >> 16) & 0xffff, guid & 0xffff);
   return buf;
}


int read_blockheader(int fd, struct fsh_block_header *hdr)
{
   if (read(fd, hdr, sizeof(*hdr)) == -1)
      perror("read"), exit(EXIT_FAILURE);

   fprintf(stderr, "# block type = 0x%02x, len = %d, guid %s\n", hdr->type, hdr->len, guid_to_string(hdr->guid));
   return hdr->len;
}


void raycoord_norm(int32_t lat0, int32_t lon0, double *lat, double *lon)
{
   *lat = lat0 / FSH_LAT_SCALE;
   *lon = lon0 / FSH_LON_SCALE * 180.0;
}


#if 0
int read_track_meta(int fd, int *len, struct fsh_track **trk, int n)
{
   struct fsh_track_meta tm;
   double lat, lon;
   uint32_t t;
   int i;

   if (read(fd, &tm, sizeof(tm)) == -1)
      perror("read"), exit(EXIT_FAILURE);
   *len -= sizeof(tm);

   for (i = 0; i < n; i++)
      if (trk[i]->bhdr.guid == tm.guid)
      {
         trk[i]->mta = tm;
         return i;
      }

   /*
   printf("# %d, %04x, %d, %d, '%s', '%s', %s\n", tm.cnt, tm.c, tm.e, tm.g, tm.name1, tm.name2, guid_to_string(tm.guid));
   raycoord_norm(tm.lat_start, tm.lon_start, &lat, &lon);
   lat = phi_iterate_merc(el, lat) * 180 / M_PI;
   printf("# %f, %f\n", lat, lon);
   raycoord_norm(tm.lat_end, tm.lon_end, &lat, &lon);
   lat = phi_iterate_merc(el, lat) * 180 / M_PI;
   printf("# %f, %f\n", lat, lon);
   */

   return -1;
}
#endif


//#define GEN_OSM
#ifdef GEN_OSM
int osm_node_out(double lat, double lon, double depth)
{
   static int id = -1;

   printf(
         "<node id='%d' lat='%.8f' lon='%.8f' version='1'>\n"
         "   <tag k='seamark:type' v='sounding'/>\n"
         "   <tag k='seamark:sounding' v='%.1f'/>\n"
         "</node>\n", 
         id, lat, lon, depth);
   return id--;
}


int osm_way_out(int first, int last)
{
   static int id = -1;
   printf("<way id='%d' version='1'>\n", id);
   for (; first >= last; first--)
      printf("   <nd ref='%d'/>\n", first);
   printf("</way>\n");
   return id--;
}
#endif


#if 0
struct fsh_track *read_track(int fd, int *len)
{
   struct fsh_track_header hdr;
   struct fsh_track *trk;
   int i;

   if (read(fd, &hdr, sizeof(hdr)) == -1)
      perror("read"), exit(EXIT_FAILURE);
   *len -= sizeof(hdr);

   if ((trk = malloc(sizeof(*trk) + hdr.cnt * sizeof(struct fsh_track_point))) == NULL)
      perror("malloc"), exit(EXIT_FAILURE);

   memset(&trk->mta, 0, sizeof(trk->mta));
   trk->hdr = hdr;

   for (i = 0; i < hdr.cnt; i++, *len -= sizeof(struct fsh_track_point))
      read(0, &trk->pt[i], sizeof(struct fsh_track_point));

   return trk;
}
#endif


static void usage(const char *s)
{
   printf(
         "parsefsh (c) 2013 by Bernhard R. Fischer, <bf@abenteuerland.at>\n"
         "usage: %s [OPTIONS]\n"
         "   -h ............. This help.\n"
         "   -o ............. Output OSM format instead of CSV.\n",
         s);
}


int fsh_track_output(struct fsh_track *trk, int cnt)
{
   int i, j;

   for (j = 0; j < cnt; j++)
   {
      for (i = 0; i < trk[j].hdr->cnt; i++)
      {
         printf("%d, %d, %d\n", trk[j].pt[i].lat, trk[j].pt[i].lon, trk[j].pt[i].depth);
      }
   }
}


struct fsh_block *fsh_block_read(int fd)
{
   int blen, blk_cnt, running;
   struct fsh_block *blk = NULL;

   for (running = 1, blk_cnt = 0; running; blk_cnt++)
   {
      if ((blk = realloc(blk, sizeof(*blk) * (blk_cnt + 1))) == NULL)
         perror("realloc"), exit(EXIT_FAILURE);
      blk[blk_cnt].data = NULL;
      blen = read_blockheader(fd, &blk[blk_cnt].hdr);
      fprintf(stderr, "# block type 0x%02x\n", blk[blk_cnt].hdr.type);

      if (blk[blk_cnt].hdr.type == 0xffff)
      {
            fprintf(stderr, "# end\n");
            running = 0;
            continue;
      }

      if ((blk[blk_cnt].data = malloc(blk[blk_cnt].hdr.len)) == NULL)
         perror("malloc"), exit(EXIT_FAILURE);

      if (read(fd, blk[blk_cnt].data, blk[blk_cnt].hdr.len) == -1)
         perror("read"), exit(EXIT_FAILURE);
   }
   return blk;
}


int fsh_track_decode(const struct fsh_block *blk, struct fsh_track **trk)
{
   int i, trk_cnt = 0;

   for (; blk->hdr.type != 0xffff; blk++)
   {
      fprintf(stderr, "decoding 0x%02x\n", blk->hdr.type);
      switch (blk->hdr.type)
      {
         case 0x0d:
            fprintf(stderr, "# track\n");

            if ((*trk = realloc(*trk, sizeof(**trk) * (trk_cnt + 1))) == NULL)
               perror("realloc"), exit(EXIT_FAILURE);

            (*trk)[trk_cnt].bhdr = &blk->hdr;
            (*trk)[trk_cnt].hdr = blk->data;
            (*trk)[trk_cnt].pt = (struct fsh_track_point*) ((*trk)[trk_cnt].hdr + 1);
            (*trk)[trk_cnt].mta = NULL;
            trk_cnt++;
            break;

         case 0x0e:
            fprintf(stderr, "# track meta\n");

            for (i = 0; i < trk_cnt; i++)
               if ((*trk)[i].bhdr->guid == ((struct fsh_track_meta*) blk->data)->guid)
               {
                  (*trk)[i].mta = blk->data;
                  break;
               }

            if (i >= trk_cnt)
               fprintf(stderr, "# *** track not found for track meta data!\n");

            break;

         default:
            fprintf(stderr, "# block type 0x%02x not implemented yet\n", blk->hdr.type);
      }
   }

   return trk_cnt;
}


int fsh_read(int fd)
{
//   struct ellipsoid wgs84 = {6378137, 6356752.3142, 0};
   struct fsh_track *trk = NULL;
   int running = 1, type, blen, trk_cnt = 0;
   struct fsh_block_header hdr;
   struct fsh_block *blk;

 //  init_ellipsoid(&wgs84);

   read_rl90(fd);
   read_rayflob(fd);

   blk = fsh_block_read(fd);
   trk_cnt = fsh_track_decode(blk, &trk);
   fsh_track_output(trk, trk_cnt);

#if 0
   while (running)
   {
      /*blen = read_blockheader(0, &hdr);
      fprintf(stderr, "# block type 0x%02x\n", hdr.type);*/

      switch (hdr.type)
      {
         case 0x01:
            fprintf(stderr, "# waypoint (not parsed yet)\n");
            break;

         case 0x0d:
            fprintf(stderr, "# track\n");
            if ((trk = realloc(trk, sizeof(*trk) * (trk_cnt + 1))) == NULL)
               perror("realloc"), exit(EXIT_FAILURE);
            trk[trk_cnt] = read_track(fd, &blen);
            trk[trk_cnt]->bhdr = hdr;
            trk_cnt++;
            // FIXME: blen should be check
            break;

         case 0x0e:
            fprintf(stderr, "# track metadata\n");
            read_track_meta(fd, &blen, trk, trk_cnt);
            // FIXME: blen should be check
            break;

         case 0x21:
         case 0x22:
            fprintf(stderr, "# route (not parsed yet)\n");
            break;

         case 0xffff:
            fprintf(stderr, "# end\n");
            running = 0;
            break;

         default:
            fprintf(stderr, "# unknown type: 0x%02x\n", type);
      }
      lseek(fd, blen, SEEK_CUR);
   }
#endif
}


int main(int argc, char **argv)
{
   int c;

   while ((c = getopt(argc, argv, "ho")) != -1)
      switch (c)
      {
         case 'h':
            usage(argv[0]);
            return 0;

         case 'o':
            break;
      }

   fsh_read(0);

   return 0;
}

