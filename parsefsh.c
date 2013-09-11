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
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/types.h>
#include <errno.h>

#include "fshfunc.h"


#define DEGSCALE (M_PI / 180.0)
#define DEG2RAD(x) ((x) * DEGSCALE)
#define RAD2DEG(x) ((x) / DEGSCALE)
#define DEG2M(x) ((x) * 60 * 1852)
#define CELSIUS(x) ((double) (x) / 100.0 - 273.15)

#define TBUFLEN 24


enum {FMT_CSV, FMT_OSM, FMT_GPX};


struct coord
{
   double lat, lon;
};

struct pcoord
{
   double bearing, dist;
};


static FILE *logout_;


static void __attribute__((constructor)) init_log_output(void)
{
   logout_ = stderr;
}


int vlog(const char *fmt, ...)
{
   va_list ap;
   int ret;

   // safety check
   if (logout_ == NULL || fmt == NULL)
      return 0;

   fputs("# ", logout_);
   va_start(ap, fmt);
   ret = vfprintf(logout_, fmt, ap);
   va_end(ap);

   return ret;
}


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
static void hexdump(const void *buf, int len)
{
   int i;

   for (i = 0; i < len; i++)
      printf("%c%c ", hex_[(((char*) buf)[i] >> 4) & 15], hex_[((char*) buf)[i] & 15]);
   printf("\n");
}
#endif


static void gpx_start(FILE *out)
{
   fprintf(out, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
         "<gpx xmlns=\"http://www.topografix.com/GPX/1/1\" creator=\"parsefsh\" version=\"1.1\"\n"
         "   xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
         "   xsi:schemaLocation=\"http://www.topografix.com/GPX/1/1 http://www.topografix.com/GPX/1/1/gpx.xsd\">\n"
         );
}


static void gpx_end(FILE *out)
{
   fprintf(out, "</gpx>\n");
}


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


void output_osm_nodes(FILE *out, const fsh_wpt_data_t *wpd, const ellipsoid_t *el, int id, const char *wpt_type)
{
   char tbuf[TBUFLEN];
   struct coord cd;

   raycoord_norm(wpd->north, wpd->east, &cd.lat, &cd.lon);
   cd.lat = phi_iterate_merc(el, cd.lat) * 180 / M_PI;

   fsh_timetostr(&wpd->ts, tbuf, sizeof(tbuf));

   fprintf(out,
            "   <node id=\"%d\" lat=\"%.7f\" lon=\"%.7f\" timestamp=\"%s\">\n"
            "      <tag k=\"fsh:type\" v=\"%s\"/>\n"
            "      <tag k=\"name\" v=\"%.*s\"/>\n"
            "      <tag k=\"description\" v=\"%.*s\"/>\n",
            id, cd.lat, cd.lon, tbuf, wpt_type, wpd->name_len, wpd->txt_data, wpd->cmt_len, wpd->txt_data + wpd->name_len);

   if (wpd->depth != -1)
      fprintf(out, 
            "      <tag k=\"seamark:sounding\" v=\"%.1f\"/>\n"
            "      <tag k=\"seamark:type\" v=\"sounding\"/>\n",
            (double) wpd->depth / 100.0);
   if (wpd->tempr != 0xffff)
      fprintf(out, 
           "      <tag k=\"temperature\" v=\"%.1f\"/>\n",
           CELSIUS(wpd->tempr));

   fprintf(out, "   </node>\n");
}


int track_output_osm_nodes(FILE *out, track_t *trk, int cnt, const ellipsoid_t *el)
{
   fsh_wpt_data_t wpd;
   int i, j, k;

   memset(&wpd, 0, sizeof(wpd));
   wpd.tempr = 0xffff;

   for (j = 0; j < cnt; j++)
   {
      trk[j].first_id = get_id();
      for (k = 0; k < trk[j].mta->guid_cnt; k++)
         for (i = 0; i < trk[j].tseg[k].hdr->cnt; i++)
         {
            if (trk[j].tseg[k].pt[i].c == -1)
               continue;

            wpd.north = trk[j].tseg[k].pt[i].north;
            wpd.east = trk[j].tseg[k].pt[i].east;
            wpd.depth = trk[j].tseg[k].pt[i].depth;
            output_osm_nodes(out, &wpd, el, get_id() + 1, "trackpoint");
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
      if (trk[j].mta != NULL && trk[j].mta->name != NULL)
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


int track_output_gpx(FILE *out, const track_t *trk, int cnt, const ellipsoid_t *el)
{
   struct coord cd, cd0;
   int i, j, k, n;

   for (j = 0; j < cnt; j++)
   {
      fprintf(out, " <trk>\n");
      if (trk[j].mta != NULL)
      {
         fprintf(out, "  <name>%.*s</name>\n  <trkseg>\n",
               (int) sizeof(trk[j].mta->name), trk[j].mta->name != NULL ? trk[j].mta->name : "");
      }

      for (k = 0, n = 0; k < trk[j].mta->guid_cnt; k++)
         for (i = 0; i < trk[j].tseg[k].hdr->cnt; i++, n++)
         {
            if (trk[j].tseg[k].pt[i].c == -1)
               continue;

            cd0 = cd;
            raycoord_norm(trk[j].tseg[k].pt[i].north, trk[j].tseg[k].pt[i].east, &cd.lat, &cd.lon);
            cd.lat = phi_iterate_merc(el, cd.lat) * 180 / M_PI;

            if (i)
               coord_diff(&cd0, &cd);

            fprintf(out, "   <trkpt lat=\"%.8f\" lon=\"%.8f\">\n    <ele>%.1f</ele>\n   </trkpt>\n",
                  cd.lat, cd.lon, (double) trk[j].tseg[k].pt[i].depth / -100);
         }
      fprintf(out, "  </trkseg>\n </trk>\n");
   }
   return 0;
}


int track_output(FILE *out, const track_t *trk, int cnt, const ellipsoid_t *el)
{
   struct coord cd, cd0;
   struct pcoord pc = {0, 0};
   double dist;
   int i, j, k, n;

   for (j = 0; j < cnt; j++)
   {
      fprintf(out, "# ----- BEGIN TRACK -----\n");
      if (trk[j].mta != NULL)
      {
         fprintf(out, "# name = '%.*s', tempr_start = %.1f, depth_start = %d, tempr_end = %.1f, depth_end = %d, guid_cnt = %d\n",
               (int) sizeof(trk[j].mta->name), trk[j].mta->name != NULL ? trk[j].mta->name : "",
               CELSIUS(trk[j].mta->tempr_start), trk[j].mta->depth_start,
               CELSIUS(trk[j].mta->tempr_end), trk[j].mta->depth_end,
               trk[j].mta->guid_cnt);
         for (i = 0; i < trk[j].mta->guid_cnt; i++)
            fprintf(out, "# guid[%d] = %s\n", i, guid_to_string(trk[j].mta->guid[i]));
      }
      else
         fprintf(out, "# no track meta data\n");

      fprintf(out, "# CNT, NR, FSH-N, FSH-E, lat, lon, DEPTH [cm], TEMPR [C], C, bearing, distance [m], TRACKNAME\n");

      for (k = 0, n = 0; k < trk[j].mta->guid_cnt; k++)
         for (i = 0, dist = 0; i < trk[j].tseg[k].hdr->cnt; i++, n++)
         {
            if (trk[j].tseg[k].pt[i].c == -1)
               continue;

            cd0 = cd;
            raycoord_norm(trk[j].tseg[k].pt[i].north, trk[j].tseg[k].pt[i].east, &cd.lat, &cd.lon);
            cd.lat = phi_iterate_merc(el, cd.lat) * 180 / M_PI;

            if (i)
               pc = coord_diff(&cd0, &cd);

            fprintf(out, "%d, %d, %d, %d, %.8f, %.8f, %d, %.1f, %d, %.1f, %.1f",
                  n, i, trk[j].tseg[k].pt[i].north, trk[j].tseg[k].pt[i].east,
                  cd.lat, cd.lon, trk[j].tseg[k].pt[i].depth, CELSIUS(trk[j].tseg[k].pt[i].tempr),
                  trk[j].tseg[k].pt[i].c, pc.bearing, DEG2M(pc.dist));
            if (trk[j].mta)
               fprintf(out, ", %.*s\n",
                  (int) sizeof(trk[j].mta->name), trk[j].mta->name != NULL ? trk[j].mta->name : "");
            else
               fprintf(out, "\n");
            dist += pc.dist;
         }
      fprintf(out, "# total distance = %.1f nm, %.1f m\n", dist * 60, DEG2M(dist));
      fprintf(out, "# ----- END TRACK -----\n");
   }
   return 0;
}


int route_output_osm_nodes(FILE *out, route21_t *rte, int cnt, const ellipsoid_t *el)
{
   fsh_route_wpt_t *wpt;
   int i, j;

   for (j = 0; j < cnt; j++)
   {
      rte[j].first_id = get_id();
      for (i = 0, wpt = rte[j].wpt; i < rte[j].hdr3->wpt_cnt; i++)
      {
         output_osm_nodes(out, &wpt->wpt.wpd, el, get_id() + 1, "routepoint");
         wpt = (fsh_route_wpt_t*) ((char*) wpt + wpt->wpt.wpd.name_len + wpt->wpt.wpd.cmt_len + sizeof(*wpt));
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


int route_output(FILE *out, const route21_t *rte, int cnt, const ellipsoid_t *el)
{
   struct coord cd;
   fsh_route_wpt_t *wpt;
   char tbuf[64];
   int i, j;

   for (j = 0; j < cnt; j++)
   {
      fprintf(out, "# route '%.*s', guid_cnt = %d\n", rte[j].hdr->name_len, rte[j].hdr->name, rte[j].hdr->guid_cnt);
      for (i = 0; i < rte[j].hdr->guid_cnt; i++)
         fprintf(out, "#   %s\n", guid_to_string(rte[j].guid[i]));

      fprintf(out, "# lat0 = %.7f, lon0 = %.7f, lat1 = %.7f, lon1 = %.7f\n# hdr2: ",
            (double) rte[j].hdr2->lat0 / 1E7, (double) rte[j].hdr2->lon0 / 1E7,
            (double) rte[j].hdr2->lat1 / 1E7, (double) rte[j].hdr2->lon1 / 1E7);
      hexdump((char*) rte[j].hdr2 + 16, sizeof(*rte[j].hdr2) - 16);
      fprintf(out, "# hdr2 [dec]: %d, %d\n", rte[j].hdr2->a, rte[j].hdr2->c);

      for (i = 0; i < rte[j].hdr->guid_cnt; i++)
         fprintf(out, "# %d, %d, %d, %d, %d\n", rte[j].pt[i].a, rte[j].pt[i].b, rte[j].pt[i].c, rte[j].pt[i].d, rte[j].pt[i].sym);

      fprintf(out, "# wpt_cnt %d\n", rte[j].hdr3->wpt_cnt);
      fprintf(out, "# guid_cnt %d\n", rte[j].hdr->guid_cnt);

      for (i = 0, wpt = rte[j].wpt; i < rte[j].hdr3->wpt_cnt; i++)
      {
         raycoord_norm(wpt->wpt.wpd.north, wpt->wpt.wpd.east, &cd.lat, &cd.lon);
         cd.lat = phi_iterate_merc(el, cd.lat) * 180 / M_PI;
 
         fsh_timetostr(&wpt->wpt.wpd.ts, tbuf, sizeof(tbuf));
         fprintf(out, "# %s, %.7f, %.7f, %.7f, %.7f, %d, %.*s, %.*s, %s\n", guid_to_string(wpt->guid),
               wpt->wpt.lat / 1E7, wpt->wpt.lon / 1E7, cd.lat, cd.lon, wpt->wpt.wpd.sym,
               wpt->wpt.wpd.name_len, wpt->wpt.wpd.txt_data, wpt->wpt.wpd.cmt_len, wpt->wpt.wpd.txt_data + wpt->wpt.wpd.name_len, tbuf);

         wpt = (fsh_route_wpt_t*) ((char*) wpt + wpt->wpt.wpd.name_len + wpt->wpt.wpd.cmt_len + sizeof(*wpt));
      }
   }
   return 0;
}


int wpt_01_output(FILE *out, const fsh_block_t *blk)
{
   fsh_wpt01_t *wpt;
   char tbuf[64];

   fprintf(out, "# ----- BEGIN WAYPOINTS TYPE 0x01 -----\n"
                "# GUID, LAT, LON, SYM, TEMPR [C], DEPTH [cm], NAME, COMMENT, TIMESTAMP\n");
   for (; blk->hdr.type != 0xffff; blk++)
   {
      if (blk->hdr.type != 0x01)
         continue;

      wpt = blk->data;
      fsh_timetostr(&wpt->wpd.ts, tbuf, sizeof(tbuf));
      fprintf(out, "%s, %.7f, %.7f, %d, %.1f, %d, %.*s, %.*s, %s\n", guid_to_string(wpt->guid),
            0.0, 0.0,
               wpt->wpd.sym, CELSIUS(wpt->wpd.tempr), wpt->wpd.depth,
               wpt->wpd.name_len, wpt->wpd.txt_data, wpt->wpd.cmt_len, wpt->wpd.txt_data + wpt->wpd.name_len, tbuf);
   }
   fprintf(out, "# ----- END WAYPOINTS TYPE 0x01 -----\n");
   return 0;
}


int wpt_01_output_osm_nodes(FILE *out, const fsh_block_t *blk, const ellipsoid_t *el)
{
   fsh_wpt01_t *wpt;

   for (; blk->hdr.type != 0xffff; blk++)
   {
      if (blk->hdr.type != 0x01)
         continue;

      wpt = blk->data;
      output_osm_nodes(out, &wpt->wpd, el, get_id(), "waypoint");
   }
   return 0;
}


static void output_gpx_wpt(FILE *out, const fsh_wpt_data_t *wpd, const ellipsoid_t *el)
{
   struct coord cd;
   char tbuf[64];

   raycoord_norm(wpd->north, wpd->east, &cd.lat, &cd.lon);
   cd.lat = phi_iterate_merc(el, cd.lat) * 180 / M_PI;

   fsh_timetostr(&wpd->ts, tbuf, sizeof(tbuf));

   fprintf(out,
            "   <wpt lat=\"%.7f\" lon=\"%.7f\">\n"
            "      <time>%s</time>\n"
            "      <name>%.*s</name>\n"
            "      <cmt>%.*s</cmt>\n",
            //get_id() + 1, wpt->lat / 1E7, wpt->lon / 1E7, ts, wpt->name_len, wpt->name);
            cd.lat, cd.lon, tbuf, wpd->name_len, wpd->txt_data, wpd->cmt_len, wpd->txt_data + wpd->name_len);

   if (wpd->depth != -1)
      fprintf(out, 
            "      <ele>%.1f</ele>\n",
            (double) wpd->depth / -100.0);
#if 0
   if (wpt->wpd.tempr != 0xffff)
      fprintf(out, 
           "      <tag k=\"temperature\" v=\"%.1f\"/>\n",
           CELSIUS(wpt->wpd.tempr));
#endif
   fprintf(out, "   </wpt>\n");
}


int route_output_gpx_ways(FILE *out, route21_t *rte, int cnt, const ellipsoid_t *el)
{
   fsh_route_wpt_t *wpt;
   int i;

   for (int j = 0; j < cnt; j++)
   {
      fprintf(out,
            "   <rte>\n"
            "      <name>%.*s</name>\n",
            rte[j].hdr->name_len, rte[j].hdr->name);

      for (wpt = rte[j].wpt, i = 0; i < rte[j].hdr3->wpt_cnt; i++)
      {
         output_gpx_wpt(out, &wpt->wpt.wpd, el);
         wpt = (fsh_route_wpt_t*) ((char*) wpt + wpt->wpt.wpd.name_len + wpt->wpt.wpd.cmt_len + sizeof(*wpt));
      }

      fprintf(out,
            "   </rte>\n");
   }
   return 0;
}


int wpt_01_output_gpx_nodes(FILE *out, const fsh_block_t *blk, const ellipsoid_t *el)

{
   fsh_wpt01_t *wpt;

   for (; blk->hdr.type != 0xffff; blk++)
   {
      if (blk->hdr.type != 0x01)
         continue;

      wpt = blk->data;
      output_gpx_wpt(out, &wpt->wpd, el);
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
         "   -c ............. Output CSV format instead of OSM.\n"
         "   -f <format> .... Define output format. Available formats: csv, gpx, osm.\n"
         "   -h ............. This help.\n"
         "   -q ............. Quiet. No informational output.\n",
         s);
}


int main(int argc, char **argv)
{
   fsh_file_header_t fhdr;
   fsh_flob_header_t flobhdr;
   track_t *trk;
   route21_t *rte;
   fsh_block_t *blk = NULL;
   ellipsoid_t el = WGS84;
   int fd = 0, trk_cnt = 0, fmt_out = FMT_OSM, rte_cnt = 0, flob_cnt = 0;
   FILE *out = stdout;
   int c;

   while ((c = getopt(argc, argv, "cf:hq")) != -1)
      switch (c)
      {
         case 'c':
            fmt_out = FMT_CSV;
            break;
 
         case 'f':
            if (!strcasecmp(optarg, "csv"))
               fmt_out = FMT_CSV;
            else if (!strcasecmp(optarg, "osm"))
               fmt_out = FMT_OSM;
            else if (!strcasecmp(optarg, "gpx"))
               fmt_out = FMT_GPX;
            else
               fprintf(stderr, "# unknown format '%s', defaults to OSM\n", optarg);
            break;

         case 'h':
            usage(argv[0]);
            return 0;

         case 'q':
            if ((logout_ = fopen("/dev/null", "w")) == NULL)
            {
               logout_ = stderr;
               vlog("warning: failed to open /dev/null: %s\n", strerror(errno));
            }
            break;
     }

   vlog("ARCHIVE.FSH decoder (c) 2013 by Bernhard R. Fischer, 2048R/5C5FFD47 <bf@abenteuerland.at>\n");

   check_endian();
   init_ellipsoid(&el);

   if (fsh_read_file_header(fd, &fhdr) == -1)
      fprintf(stderr, "# no RL90 header\n"), exit(EXIT_FAILURE);
   vlog("filer header values 0x%04x\n", fhdr.flobs);

   while (fsh_read_flob_header(fd, &flobhdr) != -1)
   {
      vlog("flob header values 0x%04x\n", flobhdr.h & 0xffff);
      blk = fsh_block_read(fd, blk);

      // try to read next FLOB
      vlog("looking for next flob %d\n", flob_cnt);
      flob_cnt++;
      if (flob_cnt >= fhdr.flobs >> 4)
         break;
      if (lseek(fd, flob_cnt * 0x10000 + sizeof(fhdr), SEEK_SET) == -1)
         perror("fseek"), exit(EXIT_FAILURE);
   }

   rte_cnt = fsh_route_decode(blk, &rte);
   trk_cnt = fsh_track_decode(blk, &trk);

   switch (fmt_out)
   {
      default:
      case FMT_OSM:
         osm_start(out);
         wpt_01_output_osm_nodes(out, blk, &el);
         track_output_osm_nodes(out, trk, trk_cnt, &el);
         route_output_osm_nodes(out, rte, rte_cnt, &el);
         track_output_osm_ways(out, trk, trk_cnt);
         route_output_osm_ways(out, rte, rte_cnt);
         osm_end(out);
         break;

      case FMT_CSV:
         wpt_01_output(out, blk);
         track_output(out, trk, trk_cnt, &el);
         route_output(out, rte, rte_cnt, &el);
         break;

      case FMT_GPX:
         gpx_start(out);
         wpt_01_output_gpx_nodes(out, blk, &el);
         track_output_gpx(out, trk, trk_cnt, &el);
         route_output_gpx_ways(out, rte, rte_cnt, &el);
         gpx_end(out);
         break;
   }

   free(rte);
   free(trk);
   fsh_free_block_data(blk);
   free(blk);

   return 0;
}

