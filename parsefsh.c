#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/types.h>

#include "parsefsh.h"


const int RL90_HEADER_LEN = 28;
const int RFLOB_HEADER_LEN = 14;
const char *RL90  = "RL90 FLASH FILE";
const char *RFLOB = "RAYFLOB1";

static const char hex_[] = "0123456789abcdef";


static void init_ellipsoid(ellipsoid_t *el)
{
   el->e = sqrt(1 - pow(el-> b / el-> a, 2));
}


static double phi_rev_merc(const ellipsoid_t *el, double N, double phi0)
{
   double esin = el->e * sin(phi0);
   return M_PI_2 - 2.0 * atan(exp(-N / el->a) * pow((1 - esin) / (1 + esin), el->e / 2));
}


static double phi_iterate_merc(const ellipsoid_t *el, double N)
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


// only used for debugging and reverse engineering
#ifdef REVENG
static void hexdump(void *buf, int len)
{
   int i;

   for (i = 0; i < len; i++)
      printf("%c%c ", hex_[(((char*) buf)[i] >> 4) & 15], hex_[((char*) buf)[i] & 15]);
   printf("\n");
}
#endif


static char *guid_to_string(uint64_t guid)
{
   static char buf[32];

   snprintf(buf, sizeof(buf),  "%ld-%ld-%ld-%ld",
         guid >> 48, (guid >> 32) & 0xffff, (guid >> 16) & 0xffff, guid & 0xffff);
   return buf;
}


static void raycoord_norm(int32_t lat0, int32_t lon0, double *lat, double *lon)
{
   *lat = lat0 / FSH_LAT_SCALE;
   *lon = lon0 / FSH_LON_SCALE * 180.0;
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


int read_blockheader(int fd, fsh_block_header_t *hdr)
{
   if (read(fd, hdr, sizeof(*hdr)) == -1)
      perror("read"), exit(EXIT_FAILURE);

   fprintf(stderr, "# block type = 0x%02x, len = %d, guid %s\n", hdr->type, hdr->len, guid_to_string(hdr->guid));
   return hdr->len;
}


int track_output_osm(FILE *out, track_t *trk, int cnt, const ellipsoid_t *el)
{
 #define TBUFLEN 24
   char ts[TBUFLEN] = "0000-00-00T00:00:00Z";
   static int id = -1;
   double lat, lon;
   struct tm *tm;
   time_t t;
   int i, j;

   time(&t);
   if ((tm = gmtime(&t)) != NULL)
      strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", tm);

   fprintf(out, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<osm version=\"0.6\" generator=\"parsefsh\">\n");

   for (j = 0; j < cnt; j++)
   {
      trk[j].first_id = id;
      for (i = 0; i < trk[j].hdr->cnt; i++)
      {
         if (trk[j].pt[i].c == -1)
            continue;

         raycoord_norm(trk[j].pt[i].lat, trk[j].pt[i].lon, &lat, &lon);
         lat = phi_iterate_merc(el, lat) * 180 / M_PI;

         fprintf(out,
               "   <node id=\"%d\" lat=\"%.8f\" lon=\"%.8f\" version=\"1\" timestamp=\"%s\">\n"
               "      <tag k=\"seamark:type\" v=\"sounding\"/>\n"
               "      <tag k=\"seamark:sounding\" v=\"%.1f\"/>\n"
               "   </node>\n",
               id--, lat, lon, ts, (double) trk[j].pt[i].depth / 100);
      }
      trk[j].last_id = id + 1;
   }

   for (j = 0; j < cnt; j++)
   {
      fprintf(out, "   <way id=\"%d\" version =\"1\" timestamp=\"%s\">\n", id--, ts);
      fprintf(out, "      <tag k=\"name\" v=\"%s\"/>\n", trk[j].mta->name1);
      for (i = trk[j].first_id; i >= trk[j].last_id; i--)
      {
         fprintf(out, "      <nd ref=\"%d\"/>\n", i);
      }
      fprintf(out, "   </way>\n");
   }

   fprintf(out, "</osm>\n");

   return 0;
}


int track_output(FILE *out, const track_t *trk, int cnt, const ellipsoid_t *el)
{
   double lat, lon;
   int i, j;

   for (j = 0; j < cnt; j++)
   {
      for (i = 0; i < trk[j].hdr->cnt; i++)
      {
         if (trk[j].pt[i].c == -1)
            continue;

         raycoord_norm(trk[j].pt[i].lat, trk[j].pt[i].lon, &lat, &lon);
         lat = phi_iterate_merc(el, lat) * 180 / M_PI;

         fprintf(out, "%d, %d, %.8f, %.8f, %d, %s\n",
               trk[j].pt[i].lat, trk[j].pt[i].lon, lat, lon, trk[j].pt[i].depth, trk[j].mta->name1);
      }
   }
   return 0;
}


fsh_block_t *fsh_block_read(int fd)
{
   int blk_cnt, running;
   fsh_block_t *blk = NULL;

   for (running = 1, blk_cnt = 0; running; blk_cnt++)
   {
      if ((blk = realloc(blk, sizeof(*blk) * (blk_cnt + 1))) == NULL)
         perror("realloc"), exit(EXIT_FAILURE);
      blk[blk_cnt].data = NULL;
      read_blockheader(fd, &blk[blk_cnt].hdr);
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


int fsh_track_decode(const fsh_block_t *blk, track_t **trk)
{
   int i, trk_cnt = 0;

   for (; blk->hdr.type != 0xffff; blk++)
   {
      fprintf(stderr, "# decoding 0x%02x\n", blk->hdr.type);
      switch (blk->hdr.type)
      {
         case 0x0d:
            fprintf(stderr, "# track\n");

            if ((*trk = realloc(*trk, sizeof(**trk) * (trk_cnt + 1))) == NULL)
               perror("realloc"), exit(EXIT_FAILURE);

            (*trk)[trk_cnt].bhdr = (fsh_block_header_t*) &blk->hdr;
            (*trk)[trk_cnt].hdr = blk->data;
            (*trk)[trk_cnt].pt = (fsh_track_point_t*) ((*trk)[trk_cnt].hdr + 1);
            (*trk)[trk_cnt].mta = NULL;
            trk_cnt++;
            break;

         case 0x0e:
            fprintf(stderr, "# track meta\n");

            for (i = 0; i < trk_cnt; i++)
               if ((*trk)[i].bhdr->guid == ((fsh_track_meta_t*) blk->data)->guid)
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


static void usage(const char *s)
{
   printf(
         "parsefsh (c) 2013 by Bernhard R. Fischer, <bf@abenteuerland.at>\n"
         "usage: %s [OPTIONS]\n"
         "   -h ............. This help.\n"
         "   -o ............. Output OSM format instead of CSV.\n",
         s);
}


int main(int argc, char **argv)
{
   track_t *trk = NULL;
   fsh_block_t *blk;
   ellipsoid_t el = WGS84;
   int fd = 0, trk_cnt = 0, osm_out = 0;
   int c;

   while ((c = getopt(argc, argv, "ho")) != -1)
      switch (c)
      {
         case 'h':
            usage(argv[0]);
            return 0;

         case 'o':
            osm_out = 1;
            break;
      }

   read_rl90(fd);
   read_rayflob(fd);

   blk = fsh_block_read(fd);
   trk_cnt = fsh_track_decode(blk, &trk);

   init_ellipsoid(&el);

   if (osm_out)
      track_output_osm(stdout, trk, trk_cnt, &el);
   else
      track_output(stdout, trk, trk_cnt, &el);

   return 0;
}

