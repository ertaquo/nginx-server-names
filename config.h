/*
 * Simple nginx config parser.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include <string>
#include <memory>
#include <list>
#include <stack>

using namespace std;

struct ConfigEntry {
  string operand;
  list<string> arguments;
  list<ConfigEntry *> children;
  bool transparent; // for roots&includes
  string filename;
  int line_no;
  int char_no;
};

void freeConfig(ConfigEntry * config) {
  if (!config)
    return;

  config->operand.clear();

#if __cplusplus > 199711L
  config->operand.shrink_to_fit();
#endif

  if (!config->arguments.empty()) {
    for (list<string>::iterator it = config->arguments.begin(); it != config->arguments.end(); ++it) {
      it->clear();
#if __cplusplus > 199711L
      it->shrink_to_fit();
#endif
    }
    config->arguments.clear();
  }

  if (!config->children.empty()) {
    for (list<ConfigEntry *>::iterator it = config->children.begin(); it != config->children.end(); ++it) {
      freeConfig(*it);
    }
  }

  delete config;
}

// filename - only for errors
// source - config source to be parsed
ConfigEntry * parseConfig(const char * filename, const char * source) {
  stack<ConfigEntry *> entries_stack;

  ConfigEntry * root = new ConfigEntry;
  root->transparent = true;

  ConfigEntry * currRoot = root;
  ConfigEntry * currEntry = NULL;

  entries_stack.push(currRoot);

  const char * end = source + strlen(source);

  int line_no = 1, char_no = 0;

  /*
   * States:
   *   \0 - nothing
   *   c - comment
   *   o - operand
   *   a - argument
   */
  char state = 0;

  char is_string = 0, was_string = 0;
  bool is_escaped = false;
  string curr_str = "";

  for (const char * curr = source; curr != end; curr++) {
    const char * error = NULL;

    char c = *curr;

    // 17 == end of text block
    if (c == 17)
      break;

    if (c == '\n') {
      line_no++;
      char_no = 0;
    } else {
      char_no++;
    }

    if (is_string) {
      if (is_escaped) {
        curr_str += c;
        is_escaped = false;
      }
      else if (is_string == c) {
        is_string = 0;
        was_string = is_string;
      }
      else if (c == '\\') {
        is_escaped = true;
      }
      else {
        curr_str += c;
      }

      continue;
    }
    else if (was_string) {
      if (c != ' ' && c != '\t' && c != '\n') {
        error = "invalid string statement";
      }
      was_string = 0;
    }

    if (!error) {
      switch(state) {
      case 0: // nothing
        // skip whitespaces
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
        }
        // if quotes
        else if (c == '\'' || c == '"') {
          state = 'o'; // switch to operand
          is_string = c;
          curr_str = "";
        }
        // if #comment line
        else if (c == '#') {
          state = 'c'; // comment
        }
        // if block closure
        else if (c == '}') {
          if (entries_stack.size() <= 1) {
            error = "invalid block closure";
          }
          else {
            currRoot = entries_stack.top();
            entries_stack.pop();
          }
        }
        else {
          state = 'o'; // switch to operand
          curr_str = c;
        }

        break;
      // comment
      case 'c':
        // end comment with newline
        if (c == '\n') {
          state = 0;
        }

        break;
      // operand
      case 'o':
        // end operand if space
        if (c == ' ' || c == '\t') {
          // create new child to curr root
          currEntry = new ConfigEntry;
          currEntry->transparent = false;

          currEntry->filename = filename;
          currEntry->line_no = line_no;
          currEntry->char_no = char_no;

          currRoot->children.push_back(currEntry);

          currEntry->operand = curr_str;
          curr_str = "";

          state = 'a'; // argument
        }
        // entry closure
        else if (c == ';') {
          currEntry = NULL;
          state = 0;
        }
        // block beginning
        else if (c == '{') {
          entries_stack.push(currRoot);
          currRoot = currEntry;
          state = 0;
        }
        else {
          curr_str += c;
        }

        break;
      // argument
      case 'a':
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
          if (!curr_str.empty()) {
            currEntry->arguments.push_back(curr_str);
            curr_str = "";
          }
        }
        // quotes
        else if (c == '\'' || c == '"') {
          if (curr_str.empty()) {
            is_string = c;
          } else {
            error = "invalid argument";
          }
        }
        // entry end
        else if (c == ';') {
          if (!curr_str.empty()) {
            currEntry->arguments.push_back(curr_str);
          }
          currEntry = NULL;
          state = 0;
        }
        // block beginning
        else if (c == '{') {
          entries_stack.push(currRoot);
          currRoot = currEntry;
          state = 0;
        }
        else {
          curr_str += c;
        }

        break;
      }
    }

    if (error) {
      fprintf(stderr, "Parse error in %s (line %d, char %d): %s\n", filename, line_no, char_no, error);
      freeConfig(root);
      return NULL;
    }
  }

  {
    const char * error = NULL;

    if (is_string) {
      error = "unterminated string";
    }
    else if (state) {
      error = "unterminated statement";
    }
    else if (currRoot != root) {
      error = "unterminated block";
    }

    if (error) {
      fprintf(stderr, "Parse error in %s (line %d, char %d): %s\n", filename, line_no, char_no, error);
      freeConfig(root);
      return NULL;
    }
  }

  return root;
}

void listAllEntries(ConfigEntry * root, string operand, list<ConfigEntry *> & results) {
  if (root->operand == operand) {
    results.push_back(root);
  }

  if (!root->children.empty()) {
    for (list<ConfigEntry *>::iterator it = root->children.begin(); it != root->children.end(); ++it) {
      listAllEntries(*it, operand, results);
    }
  }
}

// got from http://xoomer.virgilio.it/acantato/dev/wildcard/wildmatch.html
bool __configMatchWildcard(const char * filename, const char * pattern) {
  while (*filename) {
    switch (*pattern) {
    case '?':
      if (*filename == '.')
        return false;
      break;
    case '*':
      return !*(pattern + 1) ||
        __configMatchWildcard(filename, pattern + 1) ||
        __configMatchWildcard(filename + 1, pattern);
    default:
      if (*filename != *pattern)
        return false;
    }
    ++filename;
    ++pattern;
  }
  while (*pattern == '*')
    ++pattern;
  return !*pattern;
}

ConfigEntry * loadConfig(const char * filename, bool process_includes = true, const char * base_directory = NULL) {
  FILE * f = fopen(filename, "r");
  if (!f) {
    fprintf(stderr, "Cannot open file %s\n", filename);
    return NULL;
  }

  fseek(f, 0, SEEK_END);
  long int length = ftell(f);

  if (length == 0) {
    fclose(f);

    ConfigEntry * root = new ConfigEntry;
    root->transparent = true;

    return root;
  }

  fseek(f, 0, SEEK_SET);

  char * data = new char[length];
  if ((long int)fread(data, 1, length, f) != length) {
    fprintf(stderr, "Cannot read file %s\n", filename);
    delete [] data;
    fclose(f);
    return NULL;
  }

  fclose(f);

  ConfigEntry * root = parseConfig(filename, data);
  if (!root)
    return NULL;

  if (process_includes) {
    string base_dir;
    if (base_directory) {
      base_dir = base_directory;
    }
    else {
      base_dir = filename;
      base_dir.erase(base_dir.find_last_of("\\/"));
    }

    list<ConfigEntry *> includes;
    listAllEntries(root, "include", includes);

    if (!includes.empty()) {
      for (list<ConfigEntry *>::iterator it = includes.begin(); it != includes.end(); ++it) {
        if ((*it)->arguments.empty())
          continue;

        list<string> pathes;

        string inc_path = *(*it)->arguments.begin();
        if (inc_path[0] != '/') {
          inc_path = base_dir + "/" + inc_path;
        }

        if (inc_path.find_first_of("?*") == string::npos) {
          pathes.push_back(inc_path);
        }
        else {
          string inc_dir = inc_path;
          inc_dir.erase(inc_dir.find_last_of("\\/"));

          if (inc_dir.find_first_of("?*") != string::npos) {
            fprintf(stderr, "Unsupported include wildcard: %s\n", inc_path.c_str());
            freeConfig(root);
            return NULL;
          }

          DIR * dir = opendir(inc_dir.c_str());
          if (!dir) {
            fprintf(stderr, "Cannot open directory %s (include %s)\n", inc_dir.c_str(), inc_path.c_str());
          }

          struct dirent * ent;
          while ((ent = readdir(dir)) != NULL) {
            if (ent->d_type == DT_DIR)
              continue;

            string full_path = inc_dir;
            full_path += "/";
            full_path += ent->d_name;

            if (!__configMatchWildcard(full_path.c_str(), inc_path.c_str()))
              continue;

            pathes.push_back(full_path);
          }
        }

        for (list<string>::iterator pit = pathes.begin(); pit != pathes.end(); ++pit) {
          string path = *pit;

          ConfigEntry * inc_root = loadConfig(path.c_str(), process_includes, base_dir.c_str());
          if (!inc_root) {
            fprintf(stderr, "Cannot include file %s (%s, line %d, char %d)\n", path.c_str(), (*it)->filename.c_str(), (*it)->line_no, (*it)->char_no);
            
            freeConfig(root);
            return NULL;
          }

          if (!inc_root->children.empty()) {
            for (list<ConfigEntry *>::iterator it2 = inc_root->children.begin(); it2 != inc_root->children.end(); ++it2) {
              (*it)->children.push_back(*it2);
            }
          }

          delete inc_root;
          (*it)->transparent = true;
        }
      }
    }
  }

  return root;
}

string __configEscape(string str) {
  if (str.find_first_of("\"' \t\r\n") == string::npos) {
    return str;
  }

  string result = "\"";
  for (std::string::iterator it = str.begin(); it != str.end(); ++it) {
    if (*it == '\\') {
      result += "\\\\";
    }
    else if (*it == '"') {
      result += "\\\"";
    }
    else {
      result += *it;
    }
  }
  result += '"';

  return result;
}

void dumpConfig(FILE * stream, ConfigEntry * root, int indent = 0) {
//  if (!root->operand.empty()) {
  if (!root->transparent) {
    string indent_str(indent, '\t');

    fprintf(stream, "%s%s", indent_str.c_str(), __configEscape(root->operand).c_str());

    if (!root->arguments.empty()) {
      for (list<string>::iterator it = root->arguments.begin(); it != root->arguments.end(); ++it) {
        fprintf(stream, " %s", __configEscape(*it).c_str());
      }
    }

    if (!root->children.empty()) {
      fprintf(stream, " {\n");
      for (list<ConfigEntry *>::iterator it = root->children.begin(); it != root->children.end(); ++it) {
        if (it != root->children.begin() && (!(*it)->children.empty() || (*it)->transparent)) {
          fprintf(stream, "\n");
        }

        dumpConfig(stream, *it, indent + 1);

        if (it != root->children.begin() && (!(*it)->children.empty() || (*it)->transparent)) {
          list<ConfigEntry *>::iterator next = it;
          next++;
          if (next != root->children.end() && ((*next)->children.empty() && !(*next)->transparent)) {
            fprintf(stream, "\n");
          }
        }
      }
      fprintf(stream, "%s}\n", indent_str.c_str());
    }
    else {
      fprintf(stream, ";\n");
    }
  }
  else {
    if (!root->children.empty()) {
      for (list<ConfigEntry *>::iterator it = root->children.begin(); it != root->children.end(); ++it) {
        if (it != root->children.begin() && (!(*it)->children.empty() || (*it)->transparent)) {
          fprintf(stream, "\n");
        }

        dumpConfig(stream, *it, indent);

        if (it != root->children.begin() && (!(*it)->children.empty() || (*it)->transparent)) {
          list<ConfigEntry *>::iterator next = it;
          next++;
          if (next != root->children.end() && ((*next)->children.empty() && !(*next)->transparent)) {
            fprintf(stream, "\n");
          }
        }
      }
    }

    fprintf(stream, "\n");
  }
}

#endif
