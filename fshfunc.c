#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>


#include "fshfunc.h"


char *guid_to_string(uint64_t guid)
{
   static char buf[32];

   snprintf(buf, sizeof(buf),  "%ld-%ld-%ld-%ld",
         guid >> 48, (guid >> 32) & 0xffff, (guid >> 16) & 0xffff, guid & 0xffff);
   return buf;
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
   for (*trk = NULL; blk->hdr.type != 0xffff; blk++)
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
   for (*rte = NULL; blk->hdr.type != 0xffff; blk++)
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

