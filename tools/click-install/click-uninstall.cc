/*
 * click-uninstall.cc -- uninstall Click kernel module
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "routert.hh"
#include "lexert.hh"
#include "error.hh"
#include "confparse.hh"
#include "clp.h"
#include "toolutils.hh"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>

#define HELP_OPT		300
#define VERSION_OPT		301

static Clp_Option options[] = {
  { "help", 0, HELP_OPT, 0, 0 },
  { "version", 'v', VERSION_OPT, 0, 0 },
};

static const char *program_name;

void
short_usage()
{
  fprintf(stderr, "Usage: %s [OPTION]... [ROUTERFILE]\n\
Try `%s --help' for more information.\n",
	  program_name, program_name);
}

void
usage()
{
  printf("\
`Click-uninstall' uninstalls Click from the current Linux kernel.\n\
\n\
Usage: %s [OPTION]...\n\
\n\
Options:\n\
      --help                    Print this message and exit.\n\
  -v, --version                 Print version number and exit.\n\
\n\
Report bugs to <click@pdos.lcs.mit.edu>.\n", program_name);
}

static bool
read_package_file(String filename, HashMap<String, int> &packages,
		  ErrorHandler *errh)
{
  if (!errh && access(filename.cc(), F_OK) < 0)
    return false;
  String text = file_string(filename, errh);
  const char *s = text.data();
  int pos = 0;
  int len = text.length();
  while (pos < len) {
    int start = pos;
    while (pos < len && !isspace(s[pos]))
      pos++;
    packages.insert(text.substring(start, pos - start), 0);
    pos = text.find_left('\n', pos) + 1;
  }
  return (bool)text;
}

static String
packages_to_remove(const StringMap &active_modules, const StringMap &packages)
{
  // remove extra packages
  String to_remove;
  // go over all modules; figure out which ones are Click packages
  // by checking `packages' array; mark old Click packages for removal
  for (StringMap::Iterator iter = active_modules.first(); iter; iter++)
    if (iter.value() == 0) {
      String key = iter.key();
      if (packages[key] >= 0)
	to_remove += " " + key;
      else {
	// check for removing an old archive package;
	// they are identified by a leading underscore.
	int p;
	for (p = 0; p < key.length() && key[p] == '_'; p++)
	  /* nada */;
	String s = key.substring(p);
	if (s && packages[s] >= 0)
	  to_remove += " " + key;
      }
    }
  return to_remove;
}

int
main(int argc, char **argv)
{
  String::static_initialize();
  ErrorHandler::static_initialize(new FileErrorHandler(stderr));
  ErrorHandler *errh = new PrefixErrorHandler
    (ErrorHandler::default_handler(), "click-uninstall: ");

  // read command line arguments
  Clp_Parser *clp =
    Clp_NewParser(argc, argv, sizeof(options) / sizeof(options[0]), options);
  Clp_SetOptionChar(clp, '+', Clp_ShortNegated);
  program_name = Clp_ProgramName(clp);

  while (1) {
    int opt = Clp_Next(clp);
    switch (opt) {
      
     case HELP_OPT:
      usage();
      exit(0);
      break;
      
     case VERSION_OPT:
      printf("click-uninstall (Click) %s\n", VERSION);
      printf("Copyright (C) 2000 Massachusetts Institute of Technology\n\
This is free software; see the source for copying conditions.\n\
There is NO warranty, not even for merchantability or fitness for a\n\
particular purpose.\n");
      exit(0);
      break;
      
     case Clp_NotOption:
     case Clp_BadOption:
      short_usage();
      exit(1);
      break;
      
     case Clp_Done:
      goto done;
      
    }
  }
  
 done:
  // do nothing if Click not installed
  if (access("/proc/click", F_OK) < 0)
    exit(0);
  
  // first, write nothing to /proc/click/config -- frees up modules
  FILE *f = fopen("/proc/click/config", "w");
  if (!f)
    errh->fatal("can't install blank configuration: %s", strerror(errno));
  fputs("// nothing\n", f);
  fclose(f);

  // find current packages
  HashMap<String, int> active_modules(-1);
  HashMap<String, int> packages(-1);
  read_package_file("/proc/modules", active_modules, errh);
  read_package_file("/proc/click/packages", packages, errh);

  // remove unused packages
  String to_remove = packages_to_remove(active_modules, packages);
  if (to_remove) {
    String cmdline = "/sbin/rmmod " + to_remove + " 2>/dev/null";
    (void) system(cmdline);
  }
  (void) system("/sbin/rmmod click");

  // see if we successfully removed it
  // wait some time before complaining in case rmmod is slow
  int tries = 0;
  while (tries < 3 && access("/proc/click", F_OK) >= 0) {
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 200000;
    select(0, 0, 0, 0, &tv);
  }
  if (access("/proc/click", F_OK) >= 0)
    errh->fatal("could not uninstall Click module");
  
  return 0;
}
