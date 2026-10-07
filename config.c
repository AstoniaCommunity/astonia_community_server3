#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "config.h"

#define MAXNAME 80
#define MAXVALUE 200
#define MAXLINE 300

struct config_data config_data = {
    .dbhost = "localhost",
    .dbuser = "root",
    .dbpass = "tgbdwf3h",
    .dbname = "merc",
    .chathost = "localhost",
    .svrkey = "4241"};

void config_set_name(char *name, char *value) {
    if (!strcmp(name, "dbhost")) {
        config_data.dbhost = strdup(value);
        return;
    }
    if (!strcmp(name, "dbuser")) {
        config_data.dbuser = strdup(value);
        return;
    }
    if (!strcmp(name, "dbpass")) {
        config_data.dbpass = strdup(value);
        return;
    }
    if (!strcmp(name, "dbname")) {
        config_data.dbname = strdup(value);
        return;
    }
    if (!strcmp(name, "chathost")) {
        config_data.chathost = strdup(value);
        return;
    }
    if (!strcmp(name, "svrkey")) {
        config_data.svrkey = strdup(value);
        return;
    }

    fprintf(stderr, "config_set: unknown name '%s'.\n", name);
    exit(1);
}

// accepts name = value
void config_string(char *buf) {
    char name[MAXNAME], value[MAXVALUE], *ptr, quote = 0, c;
    int n;

    for (ptr = buf; isspace(*ptr);) ptr++;
    if (*ptr == 0 || *ptr == '#') return; // ignore empty lines and comments

    if (*ptr == '"' || *ptr == '\'') quote = *ptr++;

    for (n = 0; n < MAXNAME; n++) {
        if (n == MAXNAME - 1) {
            fprintf(stderr, "config_set: name exceeds max length(%d).\n", MAXNAME);
            exit(1);
        }
        if (!ptr[n]) {
            fprintf(stderr, "config_set: input ends early when parsing name.\n");
            exit(1);
        }
        if (ptr[n] == '=' || isspace(ptr[n])) break;
        name[n] = tolower(ptr[n]);
    }
    name[n] = 0;
    ptr += n;

    while (isspace(*ptr)) ptr++;
    if (*ptr != '=') {
        fprintf(stderr, "config_set: missing '='.\n");
        exit(1);
    } else ptr++;
    while (isspace(*ptr)) ptr++;

    if (*ptr == '"' || *ptr == '\'') quote = *ptr++;

    for (n = 0; n < MAXVALUE; n++) {
        if (n == MAXVALUE - 1) {
            fprintf(stderr, "config_set: value exceeds max length(%d).\n", MAXVALUE);
            exit(1);
        }
        c = ptr[n];

        if (c == '\n') c = 0;
        if (!quote && isspace(c)) break;
        if (c == quote) break;

        if (!c) {
            if (quote) {
                fprintf(stderr, "config_set: value misses closing quotes..\n");
                exit(1);
            }
            break;
        }
        value[n] = ptr[n];
    }
    value[n] = 0;

    config_set_name(name, value);
}

void config_file(char *file) {
    FILE *fp;
    char buf[MAXLINE];

    if (!(fp = fopen(file, "r"))) {
        fprintf(stderr, "Could not open config file '%s'.\n", file);
        exit(1);
    }

    while (fgets(buf, MAXLINE, fp))
        config_string(buf);

    fclose(fp);
}

void config_getenv(void) {
    char *tmp;

    // copy the values, the server overwrites its argument (and thus possibly environment) space with status info

    if ((tmp = getenv("AS3_DBHOST"))) config_data.dbhost = strdup(tmp);
    if ((tmp = getenv("AS3_DBUSER"))) config_data.dbuser = strdup(tmp);
    if ((tmp = getenv("AS3_DBPASS"))) config_data.dbpass = strdup(tmp);
    if ((tmp = getenv("AS3_DBNAME"))) config_data.dbname = strdup(tmp);
    if ((tmp = getenv("AS3_CHATHOST"))) config_data.chathost = strdup(tmp);
    if ((tmp = getenv("AS3_SVRKEY"))) config_data.svrkey = strdup(tmp);
}
