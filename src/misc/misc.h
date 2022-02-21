#ifndef MISC_H
#define MISC_H

#include "../core/tld-core.h"

tld_external int my_file_exists(const char* name);
tld_external int make_cmd_line(char** command, int argc,char* const argv[]);
tld_external int tlfilename(char* path, char** out);
tld_external int tldirname(char* path, char** out);

#endif
