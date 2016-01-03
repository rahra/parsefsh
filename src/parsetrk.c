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
 * along with Parsefsh. If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <math.h>

#include "admfunc.h"


#define vlog(x...) fprintf(stderr, ## x)
#define TBUFLEN 64


enum {FMT_CSV, FMT_OSM, FMT_GPX};


// only used for debugging and reverse engineering
//#define REVENG
#ifdef REVENG
/*! This function output len bytes starting at buf in hexadecimal numbers.
 */
static void hexdump(const void *buf, int len)
{
   static const char hex[] = "0123456789abcdef";
   int i;

   for (i = 0; i < len; i++)
      printf("%c%c ", hex[(((char*) buf)[i] >> 4) & 15], hex[((char*) buf)[i] & 15]);
   printf("\n");
}
#endif


void output_node(const adm_track_point_t *tp)
{
   char ts[TBUFLEN] = "";
   double tempr, depth;
   struct tm *tm;
   time_t t;

   t = tp->timestamp + ADM_EPOCH;
   if ((tm = gmtime(&t)) != NULL)
      strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", tm);

   if (tp->tempr != ADM_DEPTH_NA)
      //tempr = (double) tp->tempr / 1E7;
      tempr = tp->tempr / ADM_LON_SCALE;
   else
      tempr = NAN;

   if (tp->depth != ADM_DEPTH_NA)
      depth = ADM_DEPTH(tp->depth);
   else
      depth = NAN;

   printf("%s,%.4f,%.4f,%.1f,%.1f\n",
         ts, tp->lat / ADM_LAT_SCALE, tp->lon / ADM_LON_SCALE, depth / 100, tempr);
}


void output_osm_node(const adm_track_point_t *tp)
{
   char ts[TBUFLEN] = "";
   static int id = 0;
   struct tm *tm;
   time_t t;

   t = tp->timestamp + ADM_EPOCH;
   if ((tm = gmtime(&t)) != NULL)
      strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", tm);

   printf("<node id='%d' timestamp='%s' version='1' lat='%.7f' lon='%.7f'>\n"
          "<tag k='seamark:sounding' v='%.1f'/>\n"
          "<tag k='seamark:type' v='sounding'/>\n</node>\n",
         --id, ts, tp->lat / ADM_LAT_SCALE, tp->lon / ADM_LON_SCALE, ADM_DEPTH(tp->depth) / 100);
}


void parse_adm(const adm_trk_header_t *th, int format)
{
   adm_trk_header2_t *th2;
   adm_track_point_t *tp;

   printf("<!-- trackname = %.*s -->\n", th->name_len, th->name);

   th2 = (adm_trk_header2_t*) ((void*) (th + 1) + th->name_len);
   tp = (adm_track_point_t*) (th2 + 1);

#ifdef REVENG
   printf("e\n");
   for (int i = 0; i < 6; i++) hexdump(&th->e[i], 4);
   printf("g\n");
   for (int i = 0; i < 10; i++) hexdump(&th->g[i], 4);
   printf("trk_header2 = $%08lx\ntrack_point[0] = $%08lx\n", (void*) th2 - (void*) th, (void*) tp - (void*)th);
#endif


   for (int i = 0; i < th2->num_tp; i++, tp++)
   {
      switch (format)
      {
         case FMT_OSM:
            output_osm_node(tp);
            break;

         case FMT_CSV:
         default:
            printf("%3d: ", i);
            output_node(tp);
      }
   }
}


void usage(const char *arg0)
{
   printf("Garmin TRK Parser, (c) 2013 by Bernhard R. Fischer, <bf@abenteuerland.at>\n"
          "usage: %s [OPTIONS]\n"
          "   -f <format> ..... <format> := 'csv' | 'osm' | 'gpx'\n",
          arg0);

}


int main(int argc, char **argv)
{
   struct stat st;
   int fd = 0;
   void *fbase;
   int format = FMT_OSM;
   int c;

   while ((c = getopt(argc, argv, "f:h")) != -1)
      switch (c)
      {
         case 'f':
            if (!strcasecmp(optarg, "csv"))
               format = FMT_CSV;
            else if (!strcasecmp(optarg, "osm"))
               format = FMT_OSM;
            else if (!strcasecmp(optarg, "gpx"))
               vlog("GPX not implemented yet!\n"), exit(1);
            else
               vlog("unknown format '%s', defaults to OSM\n", optarg);
            break;

         case 'h':
            usage(argv[0]);
            return 0;
     }


   if (fstat(fd, &st) == -1)
      perror("stat()"), exit(1);

   if ((fbase = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED)
      perror("mmap()"), exit(1);

   
   if (format == FMT_OSM)
      printf("<?xml version='1.0' encoding='UTF-8'?>\n<osm version='0.6' generator='parseadm'>\n");

   parse_adm(fbase, format);

   if (format == FMT_OSM)
      printf("</osm>\n");


   if (munmap(fbase, st.st_size) == -1)
      perror("munmap()"), exit(1);

   return 0;
}

