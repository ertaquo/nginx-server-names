#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <list>

#include "config.h"

const char * DEFAULT_CONFIG_FILE = "/etc/nginx/nginx.conf";

using namespace std;

int main(int argc, char ** argv) {
  const char * config_file = DEFAULT_CONFIG_FILE;
  bool one_line = false;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--help") == 0) {
      printf("nginx-server-names [nginx-config-file] [--one-line|-1]\n");
      printf("  Default config file: %s\n", DEFAULT_CONFIG_FILE);
      printf("  --one-one | -1   - display all server names in single line\n");
      return 0;
    }
    else if (strcmp(argv[i], "--one-line") == 0 || strcmp(argv[i], "-1") == 0) {
      one_line = true;
    }
    else {
      config_file = argv[i];
    }
  }

  ConfigEntry * root = loadConfig(config_file);

  if (!root) {
    return 1;
  }

  //dumpConfig(stdout, root);

  list<ConfigEntry *> entries;
  listAllEntries(root, "server_name", entries);

  list<string> server_names;

  if (!entries.empty())
  for (list<ConfigEntry *>::iterator it = entries.begin(); it != entries.end(); ++it) {
    if (!(*it)->arguments.empty())
    for (list<string>::iterator ait = (*it)->arguments.begin(); ait != (*it)->arguments.end(); ++ait) {
      string sname = *ait;
      bool exists = false;

      if (!server_names.empty())
      for (list<string>::iterator sit = server_names.begin(); sit != server_names.end(); ++sit) {
        if (*sit == sname) {
          exists = true;
          break;
        }
      }

      if (!exists) {
        server_names.push_back(sname);
      }
    }
  }

  if (one_line) {
    if (!server_names.empty()) {
      for (list<string>::iterator sit = server_names.begin(); sit != server_names.end(); ++sit) {
        printf("%s ", sit->c_str());
      }
      printf("\n");
    }
  }
  else {
    if (!server_names.empty()) {
      for (list<string>::iterator sit = server_names.begin(); sit != server_names.end(); ++sit) {
        printf("%s\n", sit->c_str());
      }
    }
  }

  freeConfig(root);

  return 0;
}
