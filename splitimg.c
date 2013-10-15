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
#include <fcntl.h>

#include "admfunc.h"


#define vlog(x...) fprintf(stderr, ## x)
#define TBUFLEN 64

//#define DEBUG


int write_subfile(const void *fbase, const adm_fat_t *af, const char *dir, unsigned blocksize)
{
   char name[strlen(dir) + 14];
   int i, outfd,     // output file descriptor
       fat_cnt,      // fat block counter
       len;          // length actually written, returned by write()
   size_t sub_size,  // number of bytes not yet written
          wsize;     // number of bytes which should be written by write()

   snprintf(name, sizeof(name), "%s/%.*s.%.*s",
         dir, (int) sizeof(af->sub_name), af->sub_name, (int) sizeof(af->sub_type), af->sub_type);

   if ((outfd = open(name, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR  | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)) == -1)
      return -1;

   sub_size = af->sub_size;
   for (fat_cnt = 0;;)
   {
      for (i = 0; i < MAX_FAT_BLOCKLIST && af->blocks[i] != 0xffff; i++)
      {
#ifdef DEBUG
         vlog("block[%d] = 0x%04x\n", i, af->blocks[i]);
#endif

         wsize = sub_size > blocksize ? blocksize : sub_size;
         if ((len = write(outfd, fbase + blocksize * af->blocks[i], wsize)) < (int) wsize)
         {
            if (len == -1)
               perror("write()"), exit(1);
            vlog("write() truncated, %d < %ld\n", len, wsize);
            break;
         }
         sub_size -= len;
      }

      fat_cnt++;

      // if block list of FAT was full increment to next FAT
      if (i >= MAX_FAT_BLOCKLIST)
      {
         af = (void*) af + FAT_SIZE;
         // check if next FAT belongs to same file
         if (af->next_fat)
            continue;
      }

      // break loop in all other cases
      break;
   }

   if (close(outfd) == -1)
      perror("close()");

   return fat_cnt;
}


void usage(const char *arg0)
{
   printf("Garmin IMG/ADM Splitter, (c) 2013 by Bernhard R. Fischer, <bf@abenteuerland.at>\n"
          "usage: %s [OPTIONS]\n"
          "   -d <dir> ..... Directory to extract files to.\n",
          arg0);
}


int main(int argc, char **argv)
{
   struct stat st;
   char ts[64];
   adm_header_t *ah;
   adm_fat_t *af;
   struct tm tm;
   int fd = 0;
   int blocksize;
   void *fbase;
   char *path = ".";
   int fat_cnt;
   int c;

   while ((c = getopt(argc, argv, "d:h")) != -1)
      switch (c)
      {
         case 'd':
            path = optarg;
            break;

         case 'h':
            usage(argv[0]);
            return 0;
     }


   if (fstat(fd, &st) == -1)
      perror("stat()"), exit(1);

   if ((fbase = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED)
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

   printf("signature = %s\nidentifier = %s\ncreation date = %s\n"
         "updated = %d/%d\nblock size = %d\nmap desc = %.*s\n"
         "version = %d.%d\nfat physical block = %d\n",
         ah->sig, ah->ident, ts, ah->upd_month + 1,
         ah->upd_year + (ah->upd_year >= 0x63 ? 1900 : 2000),
         blocksize, (int) sizeof(ah->map_desc), ah->map_desc,
         ah->ver_major, ah->ver_minor, ah->fat_phys_block);

   if (argc > 1)
      path = argv[1];

   af = fbase + ah->fat_phys_block * 0x200 + 0x200;
   for (; af->subfile; )
   {
      if (!af->next_fat)
      {
         printf("subfile = %d, subname = %.*s, subtype = %.*s, size = %d, nextfat = %d\n",
               af->subfile, (int) sizeof(af->sub_name), af->sub_name,
               (int) sizeof(af->sub_type), af->sub_type, af->sub_size, af->next_fat);

         if ((fat_cnt = write_subfile(fbase, af, path, blocksize)) == -1)
            perror("write_subfile()"), exit(1);

         af = (void*) af + fat_cnt * FAT_SIZE;
      }
      else
         vlog("BUG!\n");
   }

   if (munmap(fbase, st.st_size) == -1)
      perror("munmap()"), exit(1);

   return 0;
}

