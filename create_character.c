#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <getopt.h>
#include <mysql/mysql.h>
#include <mysql/mysqld_error.h>
#define EXTERNAL_PROGRAM
#include "server.h"
#undef EXTERNAL_PROGRAM
#include "drdata.h"
#include "config.h"

static MYSQL mysql;

int init_database(void) {
    // init database client
    if (!mysql_init(&mysql)) return 0;

    // try to login as root with our password
    if (!mysql_real_connect(&mysql, config_data.dbhost, config_data.dbuser, config_data.dbpass, config_data.dbname, 0, NULL, 0)) {
        fprintf(stderr, "MySQL error: %s (%d)\n", mysql_error(&mysql), mysql_errno(&mysql));
        return 0;
    }

    return 1;
}

void exit_database(void) {
    mysql_close(&mysql);
}

#define WANTSIZE 512
#define CHARINFO

int create_char(int user_ID, char *new_user, char *class) {
    char data[80];
    unsigned long long flag = 0;
    int size, expandto, add, mirror;
    char buf[WANTSIZE * 2 + 256], dbuf[WANTSIZE * 2], ddata[WANTSIZE];

    if (strlen(class) < 2) {
        fprintf(stderr, "Need at least gender and class, e.g. MW.\n");
        return 1;
    }
    for (size = 0; new_user[size]; size++) {
        if (!isalpha(new_user[size])) {
            fprintf(stderr, "The name may only contain letters.\n");
            return 1;
        }
    }
    if (size < 2 || size > 38) {
        fprintf(stderr, "The name must have between 2 and 38 letters.\n");
        return 1;
    }

    if (class[2] == 'G') flag |= CF_GOD;

    if (class[1] == 'W') flag |= CF_WARRIOR;
    else flag |= CF_MAGE;

    if (class[0] == 'M') flag |= CF_MALE;
    else flag |= CF_FEMALE;

    mysql_real_escape_string(&mysql, data, (char *)&flag, sizeof(flag));

    size = 8 + 8 + strlen(new_user) + 15 * 4 + 1 + 3 * 2 + 1 + 6 - 20;
    expandto = ((size + (WANTSIZE - 1) + 9) / WANTSIZE) * WANTSIZE;
    add = expandto - size;

    *(unsigned int *)(ddata + 0) = DRD_JUNK_PPD;
    *(unsigned int *)(ddata + 4) = add - 8;
    memset(ddata + 8, 0, add - 8);

    mysql_real_escape_string(&mysql, dbuf, ddata, add);

    mirror = RANDOM(26) + 1;

    sprintf(buf, "insert chars values ("
                 "0," // ID
                 "'%s'," // name
                 "%u," // class (aka flags)
                 "0," // karma
                 "0," // clan
                 "0," // clan rank
                 "0," // clan serial
                 "0," // experience
                 "0," // current_area
                 "1," // allowed_area
                 "%d," // creation_time
                 "1," // login_time
                 "1," // logout_time
                 "'N'," // locked
                 "%d," // sID
                 "'%s'," // chr
                 "'%s'," // item
                 "'%s'," // ppd
                 "%d," // mirror
                 "0," // current mirror
                 "1)", // spacer
            new_user,
            (unsigned int)(flag & 0xffffffff),
            (int)time(NULL),
            user_ID,
            data,
            data,
            dbuf,
            mirror);

    if (mysql_query(&mysql, buf)) {
        if (mysql_errno(&mysql) == ER_DUP_ENTRY) {
            fprintf(stderr, "Sorry, the name %s is already taken.\n", new_user);
            return 1;
        } else {
            fprintf(stderr, "Failed to create account %s: Error: %s (%d)\n", new_user, mysql_error(&mysql), mysql_errno(&mysql));
            return 2;
        }
    } else {
#ifdef CHARINFO
        sprintf(buf, "insert charinfo values ("
                     "%d," // ID
                     "'%s'," // name
                     "%u," // class (aka flags)
                     "0," // karma
                     "0," // clan
                     "0," // clan rank
                     "0," // clan serial
                     "0," // experience
                     "0," // current_area
                     "%d," // creation_time
                     "1," // login_time
                     "1," // logout_time
                     "'N'," // locked
                     "%d)", // sID
                (int)mysql_insert_id(&mysql),
                new_user,
                (unsigned int)(flag & 0xffffffff),
                (int)time(NULL),
                user_ID);
        if (mysql_query(&mysql, buf)) {
            fprintf(stderr, "Failed to create account %s: Error: %s (%d)\n", new_user, mysql_error(&mysql), mysql_errno(&mysql));
        }
#endif
    }

    return 0;
}

void help(char *prog) {
    fprintf(stderr, "Usage: %s [-s name=value] [-f filename] [-e] <accountID> <name> <genderandclassandgod>\ngenderandclassandgod: MW = male warrior, FM = female mage, FWG = female warrior god\n-s Set config name to value (e.g. dbhost=localhost).\n-f Read config file <filename>.\n-e Read configuration from environment variables.\n", prog);
}

int main(int argc, char **args) {
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

    if (argc - optind != 3) {
        help(args[0]);
        return 1;
    }

    if (!init_database()) {
        fprintf(stderr, "Cannot connect to database.\n");
        return 3;
    }

    if (!create_char(atoi(args[optind]), args[optind + 1], args[optind + 2])) {
        printf("Success.\n");
    }

    exit_database();

    return 0;
}
