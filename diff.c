#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define ARGC_ERROR 1
#define TOOMANYFILES_ERROR 2
#define CONFLICTING_OUTPUT_OPTIONS 3
#define STAT_ERROR  5

#define MAXSTRINGS 1024
#define MAXPARAS 4096

#define HASHLEN 200

#include "para.h"
#include "util.h"
#include "diff.h"

void version(void) {
  printf("\n\n\ndiff (CSUF diffutils) 1.0.0\n");
  printf("Copyright (C) 2020 CSUF\n");
  printf("This program comes with NO WARRANTY, to the extent permitted by law.\n");
  printf("You may redistribute copies of this program\n");
  printf("under the terms of the GNU General Public License.\n");
  printf("For more information about these matters, see the file named COPYING.\n");
  printf("Written by Valerie Martinez\n\n");
}

char buf[BUFLEN];
char *strings1[MAXSTRINGS], *strings2[MAXSTRINGS];
int showversion = 0, showbrief = 0, ignorecase = 0, report_identical = 0, showsidebyside = 0;
int showleftcolumn = 0, showunified = 0, showcontext = 0, suppresscommon = 0, diffnormal = 0;

int count1 = 0, count2 = 0, different = 0;

void brief(const char *filename1, const char *filename2) {
  if(para_fileequal(strings1, count1, strings2, count2) == 0) {
    printf("Files %s and %s differ\n\n", filename1, filename2);
    exit(0);
  }
}

void identicalCheck(const char *filename1, const char *filename2) {
  if(para_fileequal(strings1, count1, strings2, count2) == 1) {
    printf("Files %s and %s are identical\n\n", filename1, filename2);
    exit(0);
  }
}

void normaldiff(para* p, para* q) {
  int foundmatch = 0;
  para* qlast = q;
  int plast;
  while(p != NULL) {
    qlast = q;
    plast = p->stop;
    foundmatch = 0;
    while(q != NULL && (foundmatch = para_equal(p, q)) == 0) {
      q = para_next(q);
    }
    q = qlast;
    if(foundmatch) {
      while((foundmatch = para_equal(p, q)) == 0) {
        para_printinfo(p,q);
        para_print(q, printright);
        q = para_next(q);
        qlast = q;
      }
      if(para_equal(p,q) == 1){
        if(para_equal2(p,q)==1){
          para_printinfo(p,q);
        }
        para_printboth(p,q);
      }
      p = para_next(p);
      q = para_next(q);
    } else {
      printf("%dd%d,%d\n",(p->start+1),(q->start-1),(q->start));
      para_print(p, printleft);
      p = para_next(p);
    }
  }
  while(q != NULL) {
    printf("%da%d,%d\n",plast,q->start,q->stop);
    para_print(q, printright);
    q = para_next(q);
  }
}

char* diff_last_modified(const char* filename) {
  static char diff_formatted_lastmod[BUFLEN];
  char tmzone[10];
  struct stat sb;

  if(lstat(filename, &sb) == -1) {
    fprintf(stderr, "Could not read stat information from file: '%s'", filename);
    exit(STAT_ERROR);
  }

  memset(diff_formatted_lastmod, 0, sizeof(diff_formatted_lastmod));
  struct tm* ti = localtime(&sb.st_mtime);;                     // years start at 1900,  months zero-indexed
  struct tm* gmtimeinfo = gmtime(&sb.st_mtime);
  int gmt = gmtimeinfo->tm_hour;

  if (gmt < 12) { gmt += 24; }    // prevent wrap-around error  (e.g., gmt is 0400, but local time is 2100)

  snprintf(tmzone, sizeof(timezone), "%03d00", (ti->tm_hour - gmt));
  snprintf(diff_formatted_lastmod, sizeof(diff_formatted_lastmod), "%4d-%02d-%02d %02d:%02d:%012.9f %s",
           ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday,
           ti->tm_hour, ti->tm_min, (float)ti->tm_sec, tmzone);

  return diff_formatted_lastmod;
}

void loadfiles(const char* filename1, const char* filename2) {
  memset(buf, 0, sizeof(buf));
  memset(strings1, 0, sizeof(strings1));
  memset(strings2, 0, sizeof(strings2));

  FILE *fin1 = openfile(filename1, "r");
  FILE *fin2 = openfile(filename2, "r");

  while (!feof(fin1) && fgets(buf, BUFLEN, fin1) != NULL) { strings1[count1++] = strdup(buf); }  fclose(fin1);
  while (!feof(fin2) && fgets(buf, BUFLEN, fin2) != NULL) { strings2[count2++] = strdup(buf); }  fclose(fin2);

  if (count1 != count2) { different = 1;  return; }
  for (int i = 0, j = 0; i < count1 && j < count2;  ++i, ++j) {
    if (strcmp(strings1[i], strings2[j]) != 0) { different = 1;  return; }
  }
}

void print_option(const char* name, int value) { printf("%17s: %s\n", name, yesorno(value)); }

void diff_output_conflict_error(void) {
  fprintf(stderr, "diff: conflicting output style options\n");
  fprintf(stderr, "diff: Try `diff --help' for more information.)\n");
  exit(CONFLICTING_OUTPUT_OPTIONS);
}

void setoption(const char* arg, const char* s, const char* t, int* value) {
  if ((strcmp(arg, s) == 0) || ((t != NULL && strcmp(arg, t) == 0))) {
    *value = 1;
  }
}

void showoptions(const char* file1, const char* file2) {
  printf("diff options...\n");
  print_option("diffnormal", diffnormal);
  print_option("show_version", showversion);
  print_option("show_brief", showbrief);
  print_option("ignorecase", ignorecase);
  print_option("report_identical", report_identical);
  print_option("show_sidebyside", showsidebyside);
  print_option("show_leftcolumn", showleftcolumn);
  print_option("suppresscommon", suppresscommon);
  print_option("showcontext", showcontext);
  print_option("show_unified", showunified);

  printf("file1: %s,  file2: %s\n\n\n", file1, file2);

  printline();
}


void init_options_files(int argc, const char* argv[]) {
  int cnt = 0;
  const char* files[2] = { NULL, NULL };

  while (argc-- > 0) {
    const char* arg = *argv;
    setoption(arg, "-v",       "--version",                  &showversion);
    setoption(arg, "-q",       "--brief",                    &showbrief);
    setoption(arg, "-i",       "--ignore-case",              &ignorecase);
    setoption(arg, "-s",       "--report-identical-files",   &report_identical);
    setoption(arg, "--normal", NULL,                         &diffnormal);
    setoption(arg, "-y",       "--side-by-side",             &showsidebyside);
    setoption(arg, "--left-column", NULL,                    &showleftcolumn);
    setoption(arg, "--suppress-common-lines", NULL,          &suppresscommon);
    setoption(arg, "-c",       "--context",                  &showcontext);
    setoption(arg, "-u",       "showunified",                &showunified);
    if (arg[0] != '-') {
      if (cnt == 2) {
        fprintf(stderr, "sorry, this version of diff only handles two files\n");
        fprintf(stderr, "Usage: ./diff [options] file1 file2\n");
        exit(TOOMANYFILES_ERROR);
      } else { files[cnt++] = arg; }
    }
    ++argv;   // DEBUG only;  move increment up to top of switch at release
  }

  if (!showcontext && !showunified && !showsidebyside && !showleftcolumn) {
    diffnormal = 1;
  }

  if (showversion) { version();  exit(0); }

  if (((showsidebyside || showleftcolumn) && (diffnormal || showcontext || showunified)) ||
      (showcontext && showunified) || (diffnormal && (showcontext || showunified))) {

    diff_output_conflict_error();
  }

//  showoptions(files[0], files[1]);
  loadfiles(files[0], files[1]);

  // if (report_identical && !different) { printf("The files are identical.\n\n");   exit(0); }
  //
  // if (showbrief && different) { printf("The files are different.\n\n");   exit(0); }

  if (report_identical && !different) { identicalCheck(files[0],files[1]); }
  if (showbrief) { brief(files[0],files[1]); exit(0); }
  if(showcontext || showunified) {
    if(showcontext) {
      char * fileone = diff_last_modified(files[0]);
      char * filetwo = diff_last_modified(files[1]);
      printf("*** %s %s\n", files[0], fileone);
      printf("--- %s %s\n", files[1], filetwo);
      para* p = para_first(strings1, count1);
      para* q = para_first(strings2, count2);
      normaldiff(p,q);
      exit(0);
    } else {
      char * fileone = diff_last_modified(files[0]);
      char * filetwo = diff_last_modified(files[1]);
      printf("--- %s %s\n", files[0], fileone);
      printf("+++ %s %s\n", files[1], filetwo);
      para* p = para_first(strings1, count1);
      para* q = para_first(strings2, count2);
      normaldiff(p,q);
      exit(0);
    }
  }
  if(diffnormal) {
    para* p = para_first(strings1, count1);
    para* q = para_first(strings2, count2);
    normaldiff(p,q);
    exit(0);
  }
}


int main(int argc, const char * argv[]) {
  init_options_files(--argc, ++argv);

//  para_printfile(strings1, count1, printleft);
//  para_printfile(strings2, count2, printright);

  para* p = para_first(strings1, count1);
  para* q = para_first(strings2, count2);
  int foundmatch = 0;

  para* qlast = q;
  while (p != NULL) {
    qlast = q;
    foundmatch = 0;
    while (q != NULL && (foundmatch = para_equal(p, q)) == 0) {
      q = para_next(q);
    }
    q = qlast;

    if (foundmatch) {
      while ((foundmatch = para_equal(p, q)) == 0) {
        para_print(q, printright);
        q = para_next(q);
        qlast = q;
      }
      para_printboth(p, q);
      p = para_next(p);
      q = para_next(q);
    } else {
      para_print(p, printleft);
      p = para_next(p);
    }
  }
  while (q != NULL) {
    para_print(q, printright);
    q = para_next(q);
  }

  return 0;
}
