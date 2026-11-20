#ifndef MOCK_EXWORD_H
#define MOCK_EXWORD_H
int exword_sys_open(const char *,int);
int exword_sys_close(int);
int exword_sys_get_filesize(int);
int exword_sys_read(int,void *,int);
int exword_sys_seek(int,int,int);
int exword_sys_findfirst(const char *,int *,char *,unsigned long *);
int exword_sys_findnext(int,char *,unsigned long *);
int exword_sys_findclose(int);
#endif
