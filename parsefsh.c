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

/*! This file contains the main() function, the output functions and the
 *  functions for the coordinate transformations.
 *
 *  @author Bernhard R. Fischer
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/types.h>

#include "fshfunc.h"


#define DEGSCALE (M_PI / 180.0)
#define DEG2RAD(x) ((x) * DEGSCALE)
#define RAD2DEG(x) ((x) / DEGSCALE)
#define DEG2M(x) ((x) * 60 * 1852)


struct coord
{
   double lat, lon;
};

struct pcoord
{
   double bearing, dist;
};


/*! This function derives the ellipsoid parameters from the semi-major and
 * semi-minor axis.
 * @param el A pointer to an ellipsoid structure. el->a and el->b MUST be
 * pre-initialized.
 */
static void init_ellipsoid(ellipsoid_t *el)
{
   el->e = sqrt(1 - pow(el-> b / el-> a, 2));
}


/*! This function calculates the nearest geographic latitude to the reference
 * latitude phi0 from the Northing N based on the ellipsoid el. It must be
 * called iteratively to gain an appropriate accuracy.
 * @param el Pointer to the ellipsoid data.
 * @param N Northing according to the Mercator projection.
 * @param phi0 Reference latitude in radians. If phi0 = 0 (e.g. initially) a
 * spherical reverse Mercator is calculated.
 * @return Returns the latitude in radians.
 */
static double phi_rev_merc(const ellipsoid_t *el, double N, double phi0)
{
   double esin = el->e * sin(phi0);
   return M_PI_2 - 2.0 * atan(exp(-N / el->a) * pow((1 - esin) / (1 + esin), el->e / 2));
}


/*! This function derives the geographic latitude from the Mercator Northing N.
 * It iteratively calls phi_rev_merc(). At a maximum it iterates either MAX_IT
 * times or until the accuracy is less than IT_ACCURACY.
 * @param el Pointer to the ellipsoid data.
 * @param N Mercator Northing.
 * @return Returns the latitude in radians.
 */
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


/*! Calculate bearing and distance from src to dst.
 *  @param src Source coodinates (struct coord).
 *  @param dst Destination coordinates (struct coord).
 *  @return Returns a struct pcoord. Pcoord contains the orthodrome distance in
 *  degrees and the bearing, 0 degress north, clockwise.
 */
static struct pcoord coord_diff(const struct coord *src, const struct coord *dst)
{
   struct pcoord pc;
   double dlat, dlon;

   dlat = dst->lat - src->lat;
   dlon = (dst->lon - src->lon) * cos(DEG2RAD((src->lat + dst->lat) / 2.0));

   pc.bearing = RAD2DEG(atan2(dlon, dlat));
   pc.dist = RAD2DEG(acos(
      sin(DEG2RAD(src->lat)) * sin(DEG2RAD(dst->lat)) + 
      cos(DEG2RAD(src->lat)) * cos(DEG2RAD(dst->lat)) * cos(DEG2RAD(dst->lon - src->lon))));

   if (pc.bearing  < 0)
      pc.bearing += 360.0;

   return pc;
}


/*! Latitude and longitude in 0x0e track blocks are given in Mercator Northing
 * and Easting but are prescaled by some factors. This function reverses the
 * scaling.
 * @param lat0 Scaled Northing from 0x0e track point.
 * @param lon0 Scaled Easting from 0x0e track point.
 * @param lat Variable to receive result Northing.
 * @param lon Variable to receuve result Easting.
 */
static void raycoord_norm(int32_t lat0, int32_t lon0, double *lat, double *lon)
{
   *lat = lat0 / FSH_LAT_SCALE;
   *lon = lon0 / FSH_LON_SCALE * 180.0;
}


// only used for debugging and reverse engineering
#define REVENG
#ifdef REVENG
static const char hex_[] = "0123456789abcdef";

/*! This function output len bytes starting at buf in hexadecimal numbers.
 */
static void hexdump(void *buf, int len)
{
   int i;

   for (i = 0; i < len; i++)
      printf("%c%c ", hex_[(((char*) buf)[i] >> 4) & 15], hex_[((char*) buf)[i] & 15]);
   printf("\n");
}
#endif


static void osm_start(FILE *out)
{
   fprintf(out, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<osm version=\"0.6\" generator=\"parsefsh\">\n");
}


static void osm_end(FILE *out)
{
   fprintf(out, "</osm>\n");
}


static int get_id(void)
{
   static int id = 0;
   return --id;
}


 #define TBUFLEN 24
int track_output_osm_nodes(FILE *out, track_t *trk, int cnt, const ellipsoid_t *el)
{
   char ts[TBUFLEN] = "0000-00-00T00:00:00Z";
   double lat, lon;
   struct tm *tm;
   time_t t;
   int i, j;

   time(&t);
   if ((tm = gmtime(&t)) != NULL)
      strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", tm);

   for (j = 0; j < cnt; j++)
   {
      trk[j].first_id = get_id();
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
               "      <tag k=\"fsh:id\" v=\"%d:%d\"/>\n"
               "   </node>\n",
               get_id() + 1, lat, lon, ts, (double) trk[j].pt[i].depth / 100, j, i);
      }
      trk[j].last_id = get_id() + 2;
   }

   return 0;
}


int track_output_osm_ways(FILE *out, track_t *trk, int cnt)
{
   char ts[TBUFLEN] = "0000-00-00T00:00:00Z";
   struct tm *tm;
   time_t t;
   int i, j;

   time(&t);
   if ((tm = gmtime(&t)) != NULL)
      strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", tm);

   for (j = 0; j < cnt; j++)
   {
      fprintf(out, "   <way id=\"%d\" version =\"1\" timestamp=\"%s\">\n", get_id(), ts);
      fprintf(out, "      <tag k=\"name\" v=\"%s\"/>\n", trk[j].mta->name);
      fprintf(out, "      <tag k=\"fsh:type\" v=\"track\"/>\n");
      for (i = trk[j].first_id; i >= trk[j].last_id; i--)
      {
         fprintf(out, "      <nd ref=\"%d\"/>\n", i);
      }
      fprintf(out, "   </way>\n");
   }
   return 0;
}


int track_output(FILE *out, const track_t *trk, int cnt, const ellipsoid_t *el)
{
   struct coord cd, cd0;
   struct pcoord pc = {0, 0};
   double dist;
   int i, j;

   for (j = 0; j < cnt; j++)
   {
      fprintf(out, "# ----- BEGIN TRACK -----\n");
      fprintf(out, "# name = '%.*s', e = $%04x, g = $%04x, e1 = $%04x, g1 = $%04x\n",
            (int) sizeof(trk[j].mta->name), trk[j].mta->name,  trk[j].mta->e, trk[j].mta->g, trk[j].mta->e1, trk[j].mta->g1);
      fprintf(out, "# NR, FSH-N, FSH-E, lat, lon, DEPTH [cm], A, A [hex], C, bearing, distance [m], TRACKNAME\n");

      for (i = 0, dist = 0; i < trk[j].hdr->cnt; i++)
      {
         if (trk[j].pt[i].c == -1)
            continue;

         cd0 = cd;
         raycoord_norm(trk[j].pt[i].lat, trk[j].pt[i].lon, &cd.lat, &cd.lon);
         cd.lat = phi_iterate_merc(el, cd.lat) * 180 / M_PI;

         if (i)
            pc = coord_diff(&cd0, &cd);

         fprintf(out, "%d, %d, %d, %.8f, %.8f, %d, %d, $%04x, %d, %.1f, %.1f, %.*s\n",
               i, trk[j].pt[i].lat, trk[j].pt[i].lon, cd.lat, cd.lon, trk[j].pt[i].depth, trk[j].pt[i].a, trk[j].pt[i].a, trk[j].pt[i].c, pc.bearing, DEG2M(pc.dist), (int) sizeof(trk[j].mta->name), trk[j].mta->name);
         dist += pc.dist;
      }
      fprintf(out, "# total distance = %.1f nm, %.1f m\n", dist * 60, DEG2M(dist));
      fprintf(out, "# ----- END TRACK -----\n");
   }
   return 0;
}


int route_output_osm_nodes(FILE *out, route21_t *rte, int cnt)
{
   char ts[TBUFLEN] = "0000-00-00T00:00:00Z";
   fsh_wpt_t *wpt;
   struct tm *tm;
   time_t t;
   int i, j;

   time(&t);
   if ((tm = gmtime(&t)) != NULL)
      strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", tm);

   for (j = 0; j < cnt; j++)
   {
      rte[j].first_id = get_id();
      for (i = 0, wpt = rte[j].wpt; i < rte[j].hdr3->wpt_cnt; i++)
      {
         fprintf(out,
               "   <node id=\"%d\" lat=\"%.7f\" lon=\"%.7f\" timestamp=\"%s\">\n"
               "      <tag k=\"name\" v=\"%.*s\"/>\n"
               "   </node>\n",
               get_id() + 1, wpt->lat / 1E7, wpt->lon / 1E7, ts, wpt->name_len, wpt->name);
         wpt = (fsh_wpt_t*) ((char*) wpt + wpt->name_len + sizeof(*wpt));
      }
      rte[j].last_id = get_id() + 2;
   }
   return 0;
}


int route_output_osm_ways(FILE *out, route21_t *rte, int cnt)
{
   char ts[TBUFLEN] = "0000-00-00T00:00:00Z";
   struct tm *tm;
   time_t t;
   int i, j;

   time(&t);
   if ((tm = gmtime(&t)) != NULL)
      strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", tm);

   for (j = 0; j < cnt; j++)
   {
      fprintf(out,
            "   <way id=\"%d\" version =\"1\" timestamp=\"%s\">\n"
            "      <tag k=\"name\" v=\"%.*s\"/>\n"
            "      <tag k=\"fsh:type\" v=\"route\"/>\n",
            get_id(), ts, rte[j].hdr->name_len, rte[j].hdr->name);
      for (i = rte[j].first_id; i >= rte[j].last_id; i--)
         fprintf(out, "      <nd ref=\"%d\"/>\n", i);
      fprintf(out, "   </way>\n");
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


static void check_endian(void)
{
   int c = 1;

   if (!*((char*) &c))
      fprintf(stderr, "# parsefsh currently only works on little endian machines (such as Intel)\n"),
         exit(EXIT_FAILURE);
}


static void usage(const char *s)
{
   printf(
         "usage: %s [OPTIONS]\n"
         "   -h ............. This help.\n"
         "   -c ............. Output CSV format instead of OSM.\n",
         s);
}


int main(int argc, char **argv)
{
   fsh_file_header_t fhdr;
   track_t *trk;
   route21_t *rte;
   fsh_block_t *blk;
   ellipsoid_t el = WGS84;
   int fd = 0, trk_cnt = 0, osm_out = 1, rte_cnt = 0;
   FILE *out = stdout;
   int c;

   fprintf(stderr, "# ARCHIVE.FSH decoder (c) 2013 by Bernhard R. Fischer, 2048R/5C5FFD47 <bf@abenteuerland.at>\n");
   while ((c = getopt(argc, argv, "ch")) != -1)
      switch (c)
      {
         case 'c':
            osm_out = 0;
            break;
 
         case 'h':
            usage(argv[0]);
            return 0;

     }

   check_endian();
   fsh_read_file_header(fd, &fhdr);
   fprintf(stderr, "# header values 0x%08x, 0x%04x\n", fhdr.a, fhdr.h & 0xffff);

   blk = fsh_block_read(fd);

   rte_cnt = fsh_route_decode(blk, &rte);
   trk_cnt = fsh_track_decode(blk, &trk);

   init_ellipsoid(&el);

   if (osm_out)
   {
      osm_start(out);
      track_output_osm_nodes(out, trk, trk_cnt, &el);
      route_output_osm_nodes(out, rte, rte_cnt);
      track_output_osm_ways(out, trk, trk_cnt);
      route_output_osm_ways(out, rte, rte_cnt);
      osm_end(out);
   }
   else
   {
      track_output(out, trk, trk_cnt, &el);
      route_output(out, rte, rte_cnt);
   }

   free(rte);
   free(trk);
   fsh_free_block_data(blk);
   free(blk);

   return 0;
}

