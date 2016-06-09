#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>

typedef struct Name {
   char *name;
   struct Name *next;
} Name;

void ParseArgs(char **args, Name **files, char **search, char *term,
 int *dictOnly, int *ignoreCase) {
   char **arg = args + 1, *flag;
   Name *temp;

   while (*arg) {
      if (**arg == '-') {
         flag = *arg + 1;
         while (*flag) {

            switch (*flag) {

            case 'd':
               *dictOnly = 1;
               break;

            case 'f':
               *ignoreCase = 1;
               break;

            case 't':
               ++arg;
               if (!*arg || strlen(*arg) != 1) {
                  printf("look: invalid termination character\n");
                  exit(2);
               }
               else {
                  *term = **arg;
               }
               break;
            }
            ++flag;
         }
      }
      else if (!*search) {
         *search = *arg;
      }
      else {
         temp = malloc(sizeof(Name));
         temp->name = *arg;
         temp->next = *files;
         *files = temp;
      }
      ++arg;
   }

   if (!*search) {
      printf("usage: look [-df] [-t char] string [file ...]\n");
      exit(2);
   }

   if (!*files) {
      *files = malloc(sizeof(Name));
      (*files)->name = "/usr/share/dict/words";
      (*files)->next = NULL;
      *ignoreCase = 1;
      *dictOnly = 1;
   }
}

void FindLines(int fd, int *numLines, off_t **line) {
   char in, junk;

   *line = malloc(sizeof(off_t));
   *line[0] = 0;
   *numLines = 1;

   while(read(fd, &in, 1)) {
      if (in == '\n' && read(fd, &junk, 1)) {
         lseek(fd, -1, SEEK_CUR);
         ++*numLines;
         *line = realloc(*line, *numLines * sizeof(off_t));
         (*line)[*numLines - 1] = lseek(fd, 0, SEEK_CUR);
      }
   }
}

int LineCmp(int fd, int start, char *search, int dictOnly, int ignoreCase,
 char term) {
   int i, len = strlen(search);
   char in;

   lseek(fd, start, SEEK_SET);
   i = 0;
   while (i < len) {
      read(fd, &in, 1);

      if (dictOnly && !isalnum(in)) {
         ;
      }
      else if (in == term || in == '\n') {
         return in - search[i];
      }
      else if (ignoreCase) {
         if (tolower(in) - tolower(search[i])) {
            return tolower(in) - tolower(search[i]);
         }
         else {
            ++i;
         }
      }
      else {
         if (in - search[i]) {
            return in - search[i];
         }
         else {
            ++i;
         }
      }
   }
   return 0;
}

int BinSearchLine(int fd, char *search, int numLines, off_t *line, int dictOnly,
 int ignoreCase, char term) {
   int pos, m, l = 0, h = numLines - 1;

   while (l <= h) {
      m = (l + h) / 2;
      pos = LineCmp(fd, line[m], search, dictOnly, ignoreCase, term);
      if (pos == 0) {
         return m;
      }
      else if (pos < 0) {
         l = m + 1;
      }
      else {
         h = m - 1;
      }
   }
   return -1;
}

void PrintLine(int fd, int lineNum, off_t *lines) {
   char in;

   lseek(fd, lines[lineNum], SEEK_SET);
   read(fd, &in, 1);
   while (in != '\n') {
      printf("%c", in);
      read(fd, &in, 1);
   }
   printf("\n");
}

int SearchForString(int fd, int numLines, off_t *line, char *search, char term,
 int dictOnly, int ignoreCase) {
   int cmpLen = strlen(search), i, buildIndex, firstFound = -1, lineIndex,
    firstMatch, lastMatch;
   char *augSearch;

   augSearch = malloc(cmpLen + 1);
   buildIndex = 0;
   for (i = 0; i < cmpLen; i++) {
      if (dictOnly && !isalnum(search[i])) {
         ;
      }
      else if (search[i] == term) {
         augSearch[buildIndex++] = search[i];
         i = cmpLen;
      }
      else if (ignoreCase) {
         augSearch[buildIndex++] = tolower(search[i]);
      }
      else {
         augSearch[buildIndex++] = search[i];
      }
   }
   augSearch[buildIndex] = '\0';

   firstFound = BinSearchLine(fd, augSearch, numLines, line, dictOnly,
    ignoreCase, term);

   if (firstFound != -1) {
      lineIndex = firstFound;
      while (lineIndex >= 0 && !LineCmp(fd, line[lineIndex], search, dictOnly,
       ignoreCase, term)) {
         --lineIndex;
      }
      firstMatch = lineIndex + 1;

      lineIndex = firstFound;
      while (lineIndex < numLines && !LineCmp(fd, line[lineIndex], search,
       dictOnly, ignoreCase, term)) {
         ++lineIndex;
      }
      lastMatch = lineIndex - 1;

      for (lineIndex = firstMatch; lineIndex <= lastMatch; lineIndex++) {
         PrintLine(fd, lineIndex, line);
      }
   }

   free(augSearch);

   return firstFound == -1;
}

void FreeFiles(Name *head) {
   Name *temp;

   while (head) {
      temp = head->next;
      free(head);
      head = temp;
   }
}

int main(int argc, char **argv) {
   int fd, dictOnly = 0, ignoreCase = 0, numLines, ret;
   char *search = NULL, term = 0;
   Name *files = NULL, *curFile;
   off_t *line;

   ParseArgs(argv, &files, &search, &term, &dictOnly, &ignoreCase);

   curFile = files;
   while (curFile) {

      if ((fd = open(curFile->name, O_RDONLY)) < 0) {
         fprintf(stderr, "look: %s: No such file or directory\n", curFile->name);
         exit(2);
      }

      FindLines(fd, &numLines, &line);

      ret = SearchForString(fd, numLines, line, search, term, dictOnly,
       ignoreCase);

      free(line);
      FreeFiles(files);

      return ret;
   }
}