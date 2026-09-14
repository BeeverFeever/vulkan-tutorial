#ifndef STR_H
#define STR_H

#include <stddef.h>
#include <stdio.h>

#define str(cstr) str_from_cstr(cstr)

typedef struct {
   size_t length;
   char* data;
} String;

typedef String StringView;

String str_make(char *str, size_t length);
String str_from_cstr(char *cstr);

// Chop a string view up to but not including the delim.
// Only finds the first occurance of delim.
String str_chop_delim(String* in, char delim);

// Chops a string view up to the character after delim, if there is multiple consecutive delims
// it will chop at the last delim and 'discard' all of the delims.
//
// The string "hi there..... how are you?" chopped with '.' as the delim would result in "hi there"
// being returned and [in] would become " how are you?"
String str_chop_delim_consecutive(String* in, char delim);

// Same as str_chop_delim just starts from the end of the string
String str_chop_delim_reverse(String* in, char delim);

bool str_eq_cstr(String *str, char* cstr);
bool str_eq(String* str1, String* str2);

double str_strtod(String* str);
long str_strtol(String str);

int str_count_char(String str, char c);

void str_print(String str);
void str_debug(String str, char* name);

#endif
