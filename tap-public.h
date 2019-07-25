
#ifndef __TAP_PUBLIC_H__
#define __TAP_PUBLIC_H__





int
open_tap(const char *dev, int *device_handle);
void
close_tap(int device_handle);
int
execute_process(int argc, const char *argv[], int StdInputOutput);
#ifndef _WIN32
int
all_write(int fd, const unsigned char * buf, int len);
#endif




#endif
