// write kernel into floppy image


#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
int
main (int argc, char **argv) {
int fd1 = open (argv[1], O_RDONLY);
int fd2 = open (argv[2], O_CREAT | O_TRUNC | O_WRONLY);

int n, total = 0;
unsigned char buf[4096];

if(fd1 < 0 || fd2 < 0 ) {
  fprintf(stderr,"Error in opening file %d,%d\n",fd1,fd2);
  exit(1);
 }
while (1) {
n = read (fd1, buf, 4096);
total += n;
if (n <= 0) 
	break;
write (fd2, buf, n);



}
printf ("TOTAL = %d\n", total);
return (0);

}
