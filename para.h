#ifndef para_h
#define para_h

#include <stdio.h>

typedef struct para para;
struct para {
  char** base;
  int filesize;
  int start;
  int stop;
  char* firstline;   // DEBUG only
  char* secondline;
};

para* para_make(char* base[], int size, int start, int stop);
para* para_first(char* base[], int size);
para* para_next(para* p);
size_t para_filesize(para* p);
size_t para_size(para* p);
char** para_base(para* p);
char* para_info(para* p);
int   para_equal(para* p, para* q);
void para_print(para* p, void (*fp)(const char*));
void para_printfile(char* base[], int count, void (*fp)(const char*));

int para_fileequal(char* base1[], int count1, char* base2[], int count2);
void para_printboth(para* p, para* q);
void para_printinfo(para* p, para* q);
int para_equal2(para* p, para* q);
#endif /* para_h */
