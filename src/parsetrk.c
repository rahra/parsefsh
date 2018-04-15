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


static long decode_int(void *ptr, int size)
{
   long val = 0;
   for (int i = size - 1; i >= 0; i--)
   {
      val <<= 8;
      val |= *((unsigned char*) ptr + i);
   }
   return val;
}


void parse_adm(const adm_trk_header_t *th, int format)
{
   adm_descriptor_t *desc;
   adm_track_point_t *tp;
   unsigned i, hlen, numtp, namelen, dstart;
   char *name;

   printf("<!--\nheader descriptor table: %d, at offset 0x%0x\n"
         "data descriptor table: %d, at offset 0x%0x\n"
         "header: 0x%0x\n",
         th->hdr_tbl_entries, th->start_hdr_tbl,
         th->data_desc_tbl_entries, th->start_data_desc_tbl,
         th->start_hdr);

   hlen = 0;
   desc = (adm_descriptor_t*) ((char*) th + th->start_hdr_tbl);
   for (i = 0; i < th->hdr_tbl_entries; i++, desc++)
   {
      printf("type = 0x%0x, size = %d\n", desc->type, desc->size);
      switch (desc->type)
      {
         case DESC_TYPE_NAME:
            name = (char*) th + th->start_hdr + hlen;
            namelen = desc->size;
            break;

         case DESC_TYPE_NUMTP:
            numtp = decode_int((char*) th + th->start_hdr + hlen, desc->size);
            break;

         case DESC_TYPE_DATA_START:
            dstart = decode_int((char*) th + th->start_hdr + hlen, desc->size);
            tp = (adm_track_point_t*) ((char*) th + dstart);
            break;
      }
      hlen += desc->size;
   }
   desc = (adm_descriptor_t*) ((char*) th + th->start_data_desc_tbl);
   for (i = 0; i < th->data_desc_tbl_entries; i++, desc++)
   {
      printf("type = 0x%0x, size = %d\n", desc->type, desc->size);
   }

   printf("trackpoints: %d\nname: %.*s\n", numtp, namelen, name);
   printf("total header length: %ld\n-->\n",
         sizeof(*th) + hlen + sizeof(*desc) * (th->hdr_tbl_entries + th->data_desc_tbl_entries));

   for (unsigned i = 0; i < numtp; i++, tp++)
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
   printf("Garmin TRK Parser, (c) 2013-2016 by Bernhard R. Fischer, <bf@abenteuerland.at>\n"
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

