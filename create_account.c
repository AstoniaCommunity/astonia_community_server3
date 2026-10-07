#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <getopt.h>
#include <mysql/mysql.h>
#include <mysql/mysqld_error.h>
#include "argon.h"
#include "config.h"

static MYSQL mysql;

int init_database(void) {
    // init database client
    if (!mysql_init(&mysql)) return 0;

    // try to login to database using config data
    if (!mysql_real_connect(&mysql, config_data.dbhost, config_data.dbuser, config_data.dbpass, config_data.dbname, 0, NULL, 0)) {
        fprintf(stderr, "MySQL error: %s (%d)\n", mysql_error(&mysql), mysql_errno(&mysql));
        return 0;
    }

    return 1;
}

void exit_database(void) {
    mysql_close(&mysql);
}

void help(char *prog) {
    fprintf(stderr, "Usage: %s [-s name=value] [-f filename] [-e] <email> <password>\n\n-s Set config name to value (e.g. dbhost=localhost).\n-f Read config file <filename>.\n-e Read configuration from environment variables.\n", prog);
}

int main(int argc, char **args) {
    char buf[512];
    char hash[256];
    char email[80 * 2 + 1];
    int c;

    while (1) {
        c = getopt(argc, args, "hs:f:e");
        if (c == -1) break;
        switch (c) {
        case 'h':
            help(args[0]);
            exit(0);
        case 's':
            config_string(optarg);
            break;
        case 'f':
            config_file(optarg);
            break;
        case 'e':
            config_getenv();
            break;
        }
    }

    if (argc - optind != 2) {
        help(args[0]);
        return 1;
    }

    if (!init_database()) {
        fprintf(stderr, "Cannot connect to database.\n");
        return 3;
    }

    // the client only transmits MAXPASSWORD-1 (15) characters
    if (strlen(args[optind + 1]) > 15) {
        fprintf(stderr, "Password is too long, the maximum is 15 characters.\n");
        return 1;
    }
    if (strlen(args[optind]) > 80) {
        fprintf(stderr, "Email is too long.\n");
        return 1;
    }
    mysql_real_escape_string(&mysql, email, args[optind], strlen(args[optind]));

    if (argon2id_hash_password(hash, sizeof(hash), args[optind + 1], NULL)) {
        fprintf(stderr, "Argon failed. Call Dad!\n");
        return 2;
    }

    sprintf(buf, "insert subscriber (email,password,creation_time,locked,banned,vendor) values ("
                 "'%s'," // email
                 "'%s'," // password
                 "%d," // creation time
                 "'N'," // locked
                 "'I'," // banned
                 "%d)", // vendor
            email, hash, (int)time(NULL), 0);

    if (mysql_query(&mysql, buf)) {
        fprintf(stderr, "Failed to create subscriber: Error: %s (%d)", mysql_error(&mysql), mysql_errno(&mysql));
        return 2;
    }

    printf("Success. Account ID is %d.\n", (int)mysql_insert_id(&mysql));

    exit_database();

    return 0;
}
