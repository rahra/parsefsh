#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/types.h>

#include "parsefsh.h"



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
#define REVENG
#ifdef REVENG
static const char hex_[] = "0123456789abcdef";

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
      fprintf(out, "      <tag k=\"fsh:type\" v=\"track\"/>\n");
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


int route_output(FILE *out, const route21_t *rte, int cnt)
{
   fsh_wpt_t *wpt;
   time_t t;
   int i, j;

   for (j = 0; j < cnt; j++)
   {
      fprintf(out, "# route '%.*s', guid_cnt = %d\n", rte[j].hdr->name_len, rte[j].hdr->name, rte[j].hdr->guid_cnt);
      for (i = 0; i < rte[j].hdr->guid_cnt; i++)
         fprintf(out, "#   %s\n", guid_to_string(rte[j].guid[i]));

      t = rte[j].hdr2->timestamp / 10000000000L;
      fprintf(out, "# %f, %f, %s", (double) rte[j].hdr2->lat / 1E7, (double) rte[j].hdr2->lon / 1E7, ctime(&t));
      hexdump(rte[j].hdr2->d, sizeof(rte[j].hdr2->d));

      for (i = 0; i < rte[j].hdr->guid_cnt; i++)
         fprintf(out, "# %d, %d, %d, %d, %d\n", rte[j].pt[i].a, rte[j].pt[i].b, rte[j].pt[i].c, rte[j].pt[i].d, rte[j].pt[i].e);

      fprintf(out, "# wpt_cnt %d\n", rte[j].hdr3->wpt_cnt);
      fprintf(out, "# guid_cnt %d\n", rte[j].hdr->guid_cnt);

      for (i = 0, wpt = rte[j].wpt; i < rte[j].hdr3->wpt_cnt; i++)
      {
         fprintf(out, "# %s, %.7f, %.7f, %.*s\n", guid_to_string(wpt->guid),
               wpt->lat / 1E7, wpt->lon / 1E7, wpt->name_len, wpt->name);
         wpt = (fsh_wpt_t*) ((char*) wpt + wpt->name_len + sizeof(*wpt));
      }
   }
   return 0;
}


int fsh_read_file_header(int fd, fsh_file_header_t *fhdr)
{
   int len;

   if ((len = read(fd, fhdr, sizeof(*fhdr))) == -1)
      perror("read"), exit(EXIT_FAILURE);

   if (len < sizeof(*fhdr))
      fprintf(stderr, "# file header truncated, read %d of %ld\n", len, sizeof(*fhdr)),
         exit(EXIT_FAILURE);

   if (memcmp(fhdr->rl90, RL90_STR, strlen(RL90_STR)) ||
         memcmp(fhdr->rflob, RFLOB_STR, strlen(RFLOB_STR)))
      fprintf(stderr, "# file seems not to be a valid ARCHIVE.FSH!\n"),
         exit(EXIT_FAILURE);

   return 0;
}


fsh_block_t *fsh_block_read(int fd)
{
   int blk_cnt, len, pos, rlen;
   fsh_block_t *blk = NULL;

   for (blk_cnt = 0, pos = 0x2a; ; blk_cnt++)   // 0x2a is the start offset after the file header
   {
      if ((blk = realloc(blk, sizeof(*blk) * (blk_cnt + 1))) == NULL)
         perror("realloc"), exit(EXIT_FAILURE);
      blk[blk_cnt].data = NULL;

      if ((len = read(fd, &blk[blk_cnt].hdr, sizeof(blk[blk_cnt].hdr))) == -1)
         perror("read"), exit(EXIT_FAILURE);

      fprintf(stderr, "# offset = $%08x, block type = 0x%02x, len = %d, guid %s\n",
            pos, blk[blk_cnt].hdr.type, blk[blk_cnt].hdr.len, guid_to_string(blk[blk_cnt].hdr.guid));
      pos += len;

      if (len < sizeof(blk[blk_cnt].hdr))
      {
         fprintf(stderr, "# header truncated, read %d of %ld\n", len, sizeof(blk[blk_cnt].hdr));
         blk[blk_cnt].hdr.type = 0xffff;
      }

      if (blk[blk_cnt].hdr.type == 0xffff)
      {
            fprintf(stderr, "# end\n");
            break;
      }

      rlen = blk[blk_cnt].hdr.len + (blk[blk_cnt].hdr.len & 1);  // pad odd blocks by 1 byte
      if ((blk[blk_cnt].data = malloc(rlen)) == NULL)
         perror("malloc"), exit(EXIT_FAILURE);

      if ((len = read(fd, blk[blk_cnt].data, rlen)) == -1)
         perror("read"), exit(EXIT_FAILURE);
      pos += len;

      if (len < rlen)
      {
         fprintf(stderr, "# block data truncated, read %d of %d\n", len, rlen);
         // clear unfilled partition of block
         memset(blk[blk_cnt].data + len, 0, rlen - len);
         break;
      }
   }
   return blk;
}


int fsh_track_decode(const fsh_block_t *blk, track_t **trk)
{
   int i, trk_cnt = 0;

   fprintf(stderr, "# decoding tracks");
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
      }
   }

   return trk_cnt;
}


int fsh_route_decode(const fsh_block_t *blk, route21_t **rte)
{
   int rte_cnt = 0;

   fprintf(stderr, "# decoding routes");
   for (; blk->hdr.type != 0xffff; blk++)
   {
      fprintf(stderr, "# decoding 0x%02x\n", blk->hdr.type);
      switch (blk->hdr.type)
      {
         case 0x21:
            fprintf(stderr, "# route21\n");
            if ((*rte = realloc(*rte, sizeof(**rte) * (rte_cnt + 1))) == NULL)
               perror("realloc"), exit(EXIT_FAILURE);

            (*rte)[rte_cnt].bhdr = (fsh_block_header_t*) &blk->hdr;
            (*rte)[rte_cnt].hdr = blk->data;
            (*rte)[rte_cnt].guid = (int64_t*) ((char*) ((*rte)[rte_cnt].hdr + 1) + (*rte)[rte_cnt].hdr->name_len);
            (*rte)[rte_cnt].hdr2 = (struct fsh_hdr2*) ((*rte)[rte_cnt].guid + (*rte)[rte_cnt].hdr->guid_cnt);
            (*rte)[rte_cnt].pt = (struct fsh_pt*) ((*rte)[rte_cnt].hdr2 + 1);
            (*rte)[rte_cnt].hdr3 = (struct fsh_hdr3*) ((*rte)[rte_cnt].pt + (*rte)[rte_cnt].hdr->guid_cnt);
            (*rte)[rte_cnt].wpt = (fsh_wpt_t*) ((*rte)[rte_cnt].hdr3 + 1);

            rte_cnt++;
            break;
      }
   }
   return rte_cnt;
}


void fsh_free_block_data(fsh_block_t *blk)
{
   for (; blk->hdr.type != 0xffff; blk++)
      free(blk->data);
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
   fsh_file_header_t fhdr;
   track_t *trk = NULL;
   route21_t *rte = NULL;
   fsh_block_t *blk;
   ellipsoid_t el = WGS84;
   int fd = 0, trk_cnt = 0, osm_out = 0, rte_cnt = 0;
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

   fsh_read_file_header(fd, &fhdr);
   fprintf(stderr, "# header values 0x%08x, 0x%04x\n", fhdr.a, fhdr.h & 0xffff);

   blk = fsh_block_read(fd);

   rte_cnt = fsh_route_decode(blk, &rte);
   trk_cnt = fsh_track_decode(blk, &trk);

   init_ellipsoid(&el);

   if (osm_out)
      track_output_osm(stdout, trk, trk_cnt, &el);
   else
   {
      track_output(stdout, trk, trk_cnt, &el);
      route_output(stdout, rte, rte_cnt);
   }

   free(rte);
   free(trk);
   fsh_free_block_data(blk);
   free(blk);

   return 0;
}

