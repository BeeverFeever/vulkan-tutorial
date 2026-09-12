#include "str.h"

#include <stdlib.h>
#include <string.h>

String str_make(char *str, size_t length) {
   return (String){
       .length = length,
       .data = str,
   };
}

String str_from_cstr(char *cstr) {
   return (String){
       .length = strlen(cstr),
       .data = cstr,
   };
}

// Chop a string view up to but not including the delim.
// Only finds the first occurance of delim.
String str_chop_delim(String* in, char delim) {
   size_t i = 0;
   for (i = 0; (i < in->length) && (in->data[i] != delim); i++) {}

   String result = str_make(in->data, i);

   if (i < in->length) {
      in->data += i + 1;
      in->length -= i + 1;
   } else {
      in->data += i;
      in->length = 0;
   }

   return result;
}

// Chops a string view up to the character after delim, if there is multiple delims in a row it chops at the end one.
String str_chop_consecutive_delim(String* in, char delim) {
   String res = str_chop_delim(in, delim);
   size_t i = 0;
   for (i = 0; (i < in->length) && (in->data[i] == delim); i++) {}

   if (i < in->length) {
      in->data += i;
      in->length -= i;
   } else {
      in->data += i;
      in->length = 0;
   }

   return res;
}

String str_chop_delim_reverse(String* in, char delim) {
   size_t i = in->length;
   for (i; (i > 0) && (in->data[i] != delim); i--) {}

   String result = str_make(in->data + i + 1, in->length - i);

   if (i > 0) {
      // in->data += i + 1;
      in->length -= in->length - i;
   } else {
      // in->data += i;
      in->length = 0;
   }

   str_debug(result, "Result");
   return result;
}

bool str_eq_cstr(String *str, char* cstr) {
   if (str->length != strlen(cstr)) return false;
   for (size_t i = 0; i < str->length; i++) {
      if (str->data[i] != cstr[i])
         return false;
   } 
   return true;
}

bool str_eq(String* str1, String* str2) {
   if (str1->length != str2->length) { return false; }
   for (size_t i = 0; i < str1->length; i++) {
      if (str1->data[i] != str2->data[i])
         return false;
   } 
   return true;
}

double str_strtod(String* str) {
   char buf[str->length + 1];
   memcpy(buf, str->data, str->length);
   buf[str->length] = 0;
   return strtod(buf, NULL);;
}

long str_strtol(String str) {
   // there is some wacky shit going on here that I don't fully understand
   char buf[str.length + 1];
   memcpy(buf, str.data, str.length);
   buf[str.length] = 0;
   return strtol(buf, NULL, 10);
}

int str_count_char(String str, char c) {
   if (str.length == 0) return 0;
   int count = 0;
   for (size_t i = 0; i < str.length; i++) {
      if (str.data[i] == c) {
         count++;
      }
   }
   return count;
}

static char* str_to_cstr(String str) {
   return strndup(str.data, str.length);
}

void str_print(String str) {
   for (size_t i = 0; i < str.length; i++) {
      putc(str.data[i], stdout);
   }
   putc('\n', stdout);
}

void str_debug(String str, char* name) {
   char* buf;
   printf("String %s debug:\n", name);
   printf("   \"%s\"\n", (buf = str_to_cstr(str)));
   printf("   length: %lu\n", str.length);
   free(buf);
}
