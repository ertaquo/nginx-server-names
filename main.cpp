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
  bool use_depth = false;
  int depth;
  bool skip_wildcards = false;
  bool dump = false;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "-?") == 0) {
      printf("nginx-server-names [nginx-config-file] [--help|-h|-?] [--one-line|-1] [--depth <depth>|-d<depth>] \n");
      printf("                   [--skip-wildcards|-w] [--dump]\n\n");
      printf("  Default config filename: %s\n\n", DEFAULT_CONFIG_FILE);
      printf("  --help | -h | -?              - display this help\n");
      printf("  --one-one | -1                - display all server names in single line\n");
      printf("  --depth <depth> | -d<depth>   - domain name depth (1 for TLDs, 2 for domains, 3 for subdomains etc.)\n");
      printf("  --skip-wildcards | -w         - skip server names with wildcards and regular expressions \n");
      printf("                                  (RFC 1035 matching only)\n");
      printf("  --dump                        - dump entire config instead of server names\n");
      return 0;
    }
    else if (strcmp(argv[i], "--one-line") == 0 || strcmp(argv[i], "-1") == 0) {
      one_line = true;
    }
    else if (strcmp(argv[i], "--depth") == 0 || strcmp(argv[i], "-d") == 0) {
      use_depth = true;
      if (i + 1 < argc) {
        i++;
        depth = atoi(argv[i]);
      } else {
        depth = -1;
      }

    }
    // -d<depth>
    else if (argv[i][0] == '-' && argv[i][1] == 'd' && argv[i][2] >= '0' && argv[i][2] <= '9') {
      use_depth = true;
      depth = atoi(argv[i] + 2);
    }
    else if (strcmp(argv[i], "--skip-wildcards") == 0 || strcmp(argv[i], "-w") == 0) {
      skip_wildcards = true;
    }
    else if (strcmp(argv[i], "--dump") == 0) {
      dump = true;
    }
    else {
      config_file = argv[i];
    }
  }

  if (use_depth && depth < 1) {
    fprintf(stderr, "Invalid depth.\n");
    return 1;
  }

  ConfigEntry * root = loadConfig(config_file);

  if (!root) {
    return 1;
  }

  if (dump) {
    dumpConfig(stdout, root);
    freeConfig(root);
    return 1;
  }

  list<ConfigEntry *> entries;
  listAllEntries(root, "server_name", entries);

  list<string> server_names;

  if (!entries.empty())
  for (list<ConfigEntry *>::iterator it = entries.begin(); it != entries.end(); ++it) {
    if (!(*it)->arguments.empty())
    for (list<string>::iterator ait = (*it)->arguments.begin(); ait != (*it)->arguments.end(); ++ait) {
      string sname = *ait;
      bool exists = false;

      if (skip_wildcards) {
        bool valid = true;
        for (string::iterator sit = sname.begin(); sit != sname.end(); ++sit) {
          char c = *sit;
          if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-' || c == '.')
            continue;
          valid = false;
          break;
        }
        if (!valid)
          continue;
      }

      if (use_depth) {
        int sname_depth = 0;
        int i = sname.size();
        for (string::reverse_iterator rit = sname.rbegin(); rit != sname.rend(); ++rit, i--) {
          if (*rit == '.') {
            sname_depth++;
            if (sname_depth == depth) {
              sname = sname.substr(i);
              break;
            }
          }
        }
      }

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
