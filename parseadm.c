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
#include "projection.h"


#define vlog(x...) fprintf(stderr, ## x)
#define TBUFLEN 64


// only used for debugging and reverse engineering
#define REVENG
#ifdef REVENG

/*! This function output len bytes starting at buf in hexadecimal numbers.
 */
static void hexdump(const void *buf, int len)
{
   static const char hex[] = "0123456789abcdef";

   for (int i = 0; i < len; i++)
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

   //hexdump(tp, sizeof(*tp)); return;

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
         ts, tp->lat / ADM_LAT_SCALE, tp->lon / ADM_LON_SCALE, tp->depth / 100, tempr);
}


void output_osm_node(const adm_track_point_t *tp, const ellipsoid_t *el)
{
   char ts[TBUFLEN] = "";
   //static double lat = 0;
   static int id = 0;
   struct tm *tm;
   time_t t;

//   if (tp->lat != 0x69045951)
//         lat = phi_iterate_merc(el, tp->lat / ADM_LAT_SCALE) * 180 / M_PI;

   t = tp->timestamp + ADM_EPOCH;
   if ((tm = gmtime(&t)) != NULL)
      strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", tm);

   printf("<node id='%d' timestamp='%s' version='1' lat='%.7f' lon='%.7f'>\n"
          "<tag k='seamark:sounding' v='%.1f'/>\n"
          "<tag k='seamark:type' v='sounding'/>\n</node>\n",
         --id, ts, tp->lat / ADM_LAT_SCALE, tp->lon / ADM_LON_SCALE, ADM_DEPTH(tp->depth) / 100);
}


void *read_file(const void *base, const uint16_t *blocks, int num, int blocksize)
{
   void *file;

   if ((file = malloc(num * blocksize)) == NULL)
      return NULL;

   for (int i = 0; i < num; i++)
      memcpy(file + i * blocksize, base + blocks[i] * blocksize, blocksize);

   return file;
}


int main(int argc, char **argv)
{
   struct stat st;
   char ts[64];
   adm_header_t *ah;
   adm_fat_t *af;
   adm_trk_header_t *th;
   adm_trk_header2_t *th2;
   adm_track_point_t *tp;
   struct tm tm;
   int fd = 0;
   int blocksize;
   void *fbase;
   ellipsoid_t el = WGS84;

   if (fstat(fd, &st) == -1)
      perror("stat()"), exit(1);

   if ((fbase = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE | MAP_NORESERVE, fd, 0)) == MAP_FAILED)
      perror("mmap()"), exit(1);

   ah = fbase;
   memset(&tm, 0, sizeof(tm));
   tm.tm_year = ah->creat_year - 1900;
   tm.tm_mon = ah->creat_month;
   tm.tm_mday = ah->creat_day;
   tm.tm_hour = ah->creat_hour;
   tm.tm_min = ah->creat_min;
   tm.tm_sec = ah->creat_sec;
   strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", &tm);

   blocksize = 1 << (ah->blocksize_e1 + ah->blocksize_e2);

   printf("<?xml version='1.0' encoding='UTF-8'?>\n<osm version='0.6' generator='parseadm'>\n");

   printf("<!--\n");
   printf("signature = %s\nidentifier = %s\ncreation date = %s\nupdated = %d/%d\nblock size = %d\nmap desc = %.*s\n",
         ah->sig, ah->ident, ts, ah->upd_month + 1, ah->upd_year + (ah->upd_year >= 0x63 ? 1900 : 2000),
         blocksize, (int) sizeof(ah->map_desc), ah->map_desc);

   af = fbase + blocksize * 2 + 0x200;
   printf("subfile = %d\nsubname = %.*s\nsubtype = %.*s\nsize = %d\nnextfat = %d\n",
         af->subfile, (int) sizeof(af->sub_name), af->sub_name, (int) sizeof(af->sub_type), af->sub_type, af->sub_size, af->next_fat);
   for (int i = 0; i < MAX_FAT_BLOCKLIST && af->blocks[i] != 0xffff; i++)
      printf("block[%d] = 0x%04x\n", i, af->blocks[i]);

   //init_ellipsoid(&el);
   th = fbase + af->blocks[0] * blocksize;
   printf("trackname = %.*s\n", th->name_len, th->name);
   printf("-->\n");

   th2 = (adm_trk_header2_t*) ((void*) (th + 1) + th->name_len);
   tp = (adm_track_point_t*) (th2 + 1);
   for (int i = 0; i < th2->num_tp; i++, tp++)
   {
//#define OUTPUT_OSM
#ifdef OUTPUT_OSM
      output_osm_node(tp, &el);
#else
      printf("%3d: ", i);
      output_node(tp);
#endif
   }


   printf("</osm>\n");

   if (munmap(fbase, st.st_size) == -1)
      perror("munmap()"), exit(1);

   return 0;
}

