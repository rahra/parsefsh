/* Copyright 2013 Bernhard R. Fischer, 2048R/5C5FFD47 <bf@abenteuerland.at>
 *
 * This file is part of Parsefsh.
 *
 * Parsefsh is free software: you can redistribute it and/or modify
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

/*! This file contains the FSH parsing functions.
 *
 *  @author Bernhard R. Fischer
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>


#include "fshfunc.h"


#ifdef HAVE_VLOG
void vlog(const char*, ...) __attribute__((format (printf, 1, 2)));
#else
#define vlog(x...) fprintf(stderr, ## x)
#endif

char *guid_to_string(uint64_t guid)
{
   static char buf[32];

   snprintf(buf, sizeof(buf),  "%"PRIu64"-%"PRIu64"-%"PRIu64"-%"PRIu64,
         guid >> 48, (guid >> 32) & 0xffff, (guid >> 16) & 0xffff, guid & 0xffff);
   return buf;
}


/*! This function converts an FSH timestamp into string representation.
 * @param ts Pointer to FSH timestamp.
 * @param buf Pointer to buffer which will receive the string.
 * @paran len Length of buffer.
 * @return Returns the number of bytes placed into buf without the trailing \0.
 */
int fsh_timetostr(const fsh_timestamp_t *ts, char *buf, int len)
{
   time_t t = (time_t) ts->date * 3600 * 24 + ts->timeofday;
   struct tm *tm = gmtime(&t);
   return strftime(buf, len, "%Y-%m-%dT%H:%M:%SZ", tm);
}


/*! Read add parse file header.
 *  @param fd Open file descriptor.
 *  @param fhdr Pointer to fsh_file_header_t which will be filled by this
 *  function.
 *  @return Returns 0 if it es a file header (the first one). The function returns
 *  -1 in case of error. If an I/O error occurs the function does not return.
 */
int fsh_read_file_header(int fd, fsh_file_header_t *fhdr)
{
   int len;

   if ((len = read(fd, fhdr, sizeof(*fhdr))) == -1)
      perror("read"), exit(EXIT_FAILURE);

   if (len < (int) sizeof(*fhdr))
      fprintf(stderr, "# file header truncated, read %d of %d\n", len, (int) sizeof(*fhdr)),
         exit(EXIT_FAILURE);

   if (memcmp(fhdr->rl90, RL90_STR, strlen(RL90_STR)))
      return -1;

   return 0;
}


/*! This reads a flob header. It works like fsh_read_file_header(). */
int fsh_read_flob_header(int fd, fsh_flob_header_t *flobhdr)
{
   int len;

   if ((len = read(fd, flobhdr, sizeof(*flobhdr))) == -1)
      perror("read"), exit(EXIT_FAILURE);

   if (len < (int) sizeof(*flobhdr))
      fprintf(stderr, "# flob header truncated, read %d of %d\n", len, (int) sizeof(*flobhdr)),
         exit(EXIT_FAILURE);

   if (memcmp(flobhdr->rflob, RFLOB_STR, strlen(RFLOB_STR)))
      return -1;

   return 0;
}


static int fsh_block_count(const fsh_block_t *blk)
{
   int blk_cnt;

   if (blk == NULL)
      return 0;

   for (blk_cnt = 0; blk->hdr.type != FSH_BLK_ILL; blk++, blk_cnt++);

   return blk_cnt;
}


/*! This function reads all blocks into a fsh_block_t list. The type of the
 * last block (which does not contain data anymore) is set to 0xffff.
 * @return Returns a pointer to the first fsh_block_t. The list MUST be freed
 * by the caller again with a call to free and the pointer to the first block.
 */
fsh_block_t *fsh_block_read(int fd, fsh_block_t *blk)
{
   int blk_cnt, len, pos, rlen;
   //fsh_block_t *blk = NULL;
   off_t off;

   if ((off = lseek(fd, 0, SEEK_CUR)) == -1)
      perror("lseek"), exit(EXIT_FAILURE);

   blk_cnt = fsh_block_count(blk);

   for (pos = 0; ; blk_cnt++)   // 0x2a is the start offset after the file header
   {
      if ((blk = realloc(blk, sizeof(*blk) * (blk_cnt + 1))) == NULL)
         perror("realloc"), exit(EXIT_FAILURE);
      blk[blk_cnt].data = NULL;

      // check if there's enough space left in the FLOB
      if (pos + sizeof(fsh_block_header_t) + sizeof(fsh_flob_header_t) > FLOB_SIZE)
      {
         blk[blk_cnt].hdr.type = FSH_BLK_ILL;
         vlog("end, FLOB full\n");
         break;
      }

      if ((len = read(fd, &blk[blk_cnt].hdr, sizeof(blk[blk_cnt].hdr))) == -1)
         perror("read"), exit(EXIT_FAILURE);

      vlog("offset = $%08lx, pos = $%04x, block type = 0x%02x, len = %d, guid %s\n",
            pos + (long) off, pos, blk[blk_cnt].hdr.type, blk[blk_cnt].hdr.len, guid_to_string(blk[blk_cnt].hdr.guid));
      pos += len;

      if (len < (int) sizeof(blk[blk_cnt].hdr))
      {
         vlog("header truncated, read %d of %d\n", len, (int) sizeof(blk[blk_cnt].hdr));
         blk[blk_cnt].hdr.type = FSH_BLK_ILL;
      }

      if (blk[blk_cnt].hdr.type == FSH_BLK_ILL)
      {
            vlog("end, empty block\n");
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
         vlog("block data truncated, read %d of %d\n", len, rlen);
         // clear unfilled partition of block
         memset(blk[blk_cnt].data + len, 0, rlen - len);
         break;
      }
   }
   return blk;
}


// FIXME: if GUID cross pointers in FSH file are incorrect, program will not
// work correctly.
static void fsh_tseg_decode0(const fsh_block_t *blk, track_t *trk)
{
   int i;

   vlog("decoding tracks\n");
   for (; blk->hdr.type != FSH_BLK_ILL; blk++)
      if (blk->hdr.type == FSH_BLK_TRK)
         for (i = 0; i < trk->mta->guid_cnt; i++)
            if (blk->hdr.guid == trk->mta->guid[i])
            {
               trk->tseg[i].bhdr = (fsh_block_header_t*) &blk->hdr;
               trk->tseg[i].hdr = blk->data;
               trk->tseg[i].pt = (fsh_track_point_t*) (trk->tseg[i].hdr + 1);
            }
}


static void fsh_tseg_decode(const fsh_block_t *blk, track_t *trk, int trk_cnt)
{
   for (; trk_cnt; trk_cnt--, trk++)
      fsh_tseg_decode0(blk, trk);
}


/*! This function decodes track blocks (0x0d and 0x0e) into a track_t structure.
 * @param blk Pointer to the first fsh block.
 * @param trk Pointer to a track_t pointer. This variable will receive a
 * pointer to the first track. The caller must free the track list again with a
 * pointer to the first track.
 * @return Returns the number of tracks that have been decoded.
 */
static int fsh_track_decode0(const fsh_block_t *blk, track_t **trk)
{
   int trk_cnt = 0;

   vlog("decoding track metas\n");
   for (*trk = NULL; blk->hdr.type != FSH_BLK_ILL; blk++)
   {
      vlog("decoding 0x%02x\n", blk->hdr.type);
      if (blk->hdr.type == FSH_BLK_MTA)
      {
         vlog("track meta\n");

         if ((*trk = realloc(*trk, sizeof(**trk) * (trk_cnt + 1))) == NULL)
            perror("realloc"), exit(EXIT_FAILURE);

         (*trk)[trk_cnt].bhdr = (fsh_block_header_t*) &blk->hdr;
         (*trk)[trk_cnt].mta = blk->data;

         if (((*trk)[trk_cnt].tseg = malloc(sizeof(*(*trk)[trk_cnt].tseg) * (*trk)[trk_cnt].mta->guid_cnt)) == NULL)
            perror("malloc"), exit(EXIT_FAILURE);

         trk_cnt++;
      }
   }

   return trk_cnt;
}


int fsh_track_decode(const fsh_block_t *blk, track_t **trk)
{
   int trk_cnt;

   trk_cnt = fsh_track_decode0(blk, trk);
   fsh_tseg_decode(blk, *trk, trk_cnt);

   return trk_cnt;
}


/*! This function decodes route blocks (0x21) into a route21_t structure.
 * @param blk Pointer to the first fsh block.
 * @param trk Pointer to a route21_t pointer. This variable will receive a
 * pointer to the first route. The caller must free the route list again with a
 * pointer to the first route.
 * @return Returns the number of routes that have been decoded.
 */
int fsh_route_decode(const fsh_block_t *blk, route21_t **rte)
{
   int rte_cnt = 0;

   vlog("decoding routes\n");
   for (*rte = NULL; blk->hdr.type != FSH_BLK_ILL; blk++)
   {
      vlog("decoding 0x%02x\n", blk->hdr.type);
      switch (blk->hdr.type)
      {
         case FSH_BLK_RTE:
            vlog("route21\n");
            if ((*rte = realloc(*rte, sizeof(**rte) * (rte_cnt + 1))) == NULL)
               perror("realloc"), exit(EXIT_FAILURE);

            (*rte)[rte_cnt].bhdr = (fsh_block_header_t*) &blk->hdr;
            (*rte)[rte_cnt].hdr = blk->data;
            (*rte)[rte_cnt].guid = (int64_t*) ((char*) ((*rte)[rte_cnt].hdr + 1) + (*rte)[rte_cnt].hdr->name_len);
            (*rte)[rte_cnt].hdr2 = (struct fsh_hdr2*) ((*rte)[rte_cnt].guid + (*rte)[rte_cnt].hdr->guid_cnt);
            (*rte)[rte_cnt].pt = (struct fsh_pt*) ((*rte)[rte_cnt].hdr2 + 1);
            (*rte)[rte_cnt].hdr3 = (struct fsh_hdr3*) ((*rte)[rte_cnt].pt + (*rte)[rte_cnt].hdr->guid_cnt);
            (*rte)[rte_cnt].wpt = (fsh_route_wpt_t*) ((*rte)[rte_cnt].hdr3 + 1);

            rte_cnt++;
            break;
      }
   }
   return rte_cnt;
}


/*! Free all data pointers within the block list. This MUST be called before
 * the block list is freed itself.
 * @param blk Pointer to the first block.
 */
void fsh_free_block_data(fsh_block_t *blk)
{
   for (; blk->hdr.type != FSH_BLK_ILL; blk++)
      free(blk->data);
}

