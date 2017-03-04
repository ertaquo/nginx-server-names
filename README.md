nginx-server-names
==================

Lists server names from nginx config.

#### Usage ####

`nginx-server-names [nginx-config-file] [--help|-h|-?] [--one-line|-1] [--depth <depth>|-d<depth>] [--skip-wildcard|-w] [--dump]`

Default config filename: `/etc/nginx/nginx.conf`

`--help | -h | -?` - display help message

`--one-one | -1` - display all server names in single line

`--depth <depth> | -d<depth>` - domain name depth (1 for TLDs, 2 for domains, 3 for subdomains etc.)

`--skip-wildcards | -w` - skip server names with wildcards and regular expressions  (RFC 1035 matching only)

`--dump` - dump entire config instead of server names

For example, how to get LetsEncrypt certificates for all domains using certbot in single line:

``certbot certonly --webroot -w /var/www/example/ `nginx-server-names -w -d2 | sed -e 's/^/-d /' |  paste -sd ' ' -` ``

#### config.h ####

`config.h` file contains simple nginx config parser. See usage example in `main.cpp`.

Maybe it will be useful for somebody else.
