#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "at5.h"

// only used for debugging and reverse engineering
#define REVENG
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



int read_at5(void *fbase)
{

}


int main(int argc, char **argv)
{
   struct stat st;
   void *fbase;
   int fd = 0;

   if (fstat(fd, &st) == -1)
      perror("stat()"), exit(1);

   if ((fbase = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED)
      perror("mmap()"), exit(1);

   struct at5_file_header *fh = fbase;
   struct at5_h2 *fh2 = fbase + sizeof(*fh) + fh->name_len;
   printf("file length = %d\ndata length = %d\nname = %.*s\ndate = %.*s\n",
         -fh->neg_file_length - 1, fh->data_length, fh->name_len, fh->name, fh2->ds_len, fh2->date_str);

   struct at5_h3 *fh3 = (struct at5_h3*) ((char*) (fh2 + 1) + fh2->ds_len); 
   void *data = fh3 + 1;
   hexdump((void*)fh3+16, 16);
   for (int i = 0; i < 8; i++)
      hexdump(&fh3->ao0[i], 8);
   

   hexdump(data, 0x10);
   hexdump((void*)data+0x10, 0x10);

   return 0;
}


