#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <mysql/mysql.h>
#include <time.h>

struct net {
    char str[16];
    int len;
};

static int ipcmp(char *ip, struct net *network) {
    int i;

    for (i = 0; network[i].len; i++) {
	if (!strncmp(ip, network[i].str, network[i].len))
	    return 1;
    }

    return 0;
}

int main(int argc, char **argv) {
    FILE *flow;
    FILE *cfg;
    MYSQL dbh;
    char cmd[256];
    char buf[256];
    char server[64];
    char username[64];
    char password[64];
    char database[64];
    char network[64];
    int port;
    char val[64];
    struct net *networks = NULL;
    char *p;
    int i;

    if (argc != 2) {
	fprintf(stderr, "Usage: %s <flow-file>\n", argv[0]);
	exit(EXIT_FAILURE);
    }

    /* default options */
    strcpy(server, "localhost");
    strcpy(username, "root");
    strcpy(password, "");
    strcpy(database, "netflow");
    strcpy(network, "192.168.");
    port = 3306;

    /* read options */
    if ((cfg = fopen("/etc/flow-mysql.conf", "r"))) {
	while (fgets(buf, sizeof(buf), cfg)) {
	    if ((buf[0] == '#') || (buf[0] == '\n')) {
		continue;
	    } else if ((sscanf(buf, "server: %63[A-Za-z0-9 .:/_+-]", val)) == 1) {
		strcpy(server, val);
		continue;
	    } else if ((sscanf(buf, "username: %63[A-Za-z0-9 .:/_+-]", val)) == 1) {
		strcpy(username, val);
		continue;
	    } else if ((sscanf(buf, "password: %63[A-Za-z0-9 .:/_+-]", val)) == 1) {
		strcpy(password, val);
		continue;
	    } else if ((sscanf(buf, "database: %63[A-Za-z0-9 .:/_+-]", val)) == 1) {
		strcpy(database, val);
		continue;
	    } else if ((sscanf(buf, "network: %63[A-Za-z0-9 .:/_+-]", val)) == 1) {
		strcpy(network, val);
		continue;
	    } else if ((sscanf(buf, "port: %63[A-Za-z0-9 .:/_+-]", val)) == 1) {
		port = atoi(val);
		continue;
	    }
	}
	fclose(cfg);
    }

    for (i = 0, p = strtok(network, " "); p; i++, p = strtok(NULL, " ")) {
	networks = realloc(networks, sizeof(struct net) * (i + 2));
	snprintf(networks[i].str, sizeof(networks[i].str), "%s", p);
	networks[i].len = strlen(networks[i].str);
	networks[i + 1].len = 0;
    }

    openlog(PROG_NAME, LOG_PID, LOG_DAEMON);

    if (!mysql_init(&dbh)) {
	syslog(LOG_ERR, "%s", mysql_error(&dbh));
	exit(EXIT_FAILURE);
    }

    if (!mysql_real_connect(&dbh, server, username, password, database, port, NULL, 0)) {
	syslog(LOG_ERR, "%s", mysql_error(&dbh));
	exit(EXIT_FAILURE);
    }

    snprintf(cmd, sizeof(cmd), "/usr/bin/flow-export -f2 -mUNIX_SECS,DPKTS,DOCTETS,SRCADDR,DSTADDR,SRCPORT,DSTPORT,PROT < %s 2>/dev/null", argv[1]);

    if (!(flow = popen(cmd, "r"))) {
	syslog(LOG_ERR, "Error opening pipe");
	exit(EXIT_FAILURE);
    }

    while (fgets(buf, sizeof(buf), flow)) {
	if (buf[0] == '#')
	    continue;

	char date[16];
	char query[256];
	time_t unix_secs = strtol(strtok(buf, ","), NULL, 10);
	char *dpkts = strtok(NULL, ",");
	char *doctets = strtok(NULL, ",");
	char *srcaddr = strtok(NULL, ",");
	char *dstaddr = strtok(NULL, ",");
	char *srcport = strtok(NULL, ",");
	char *dstport = strtok(NULL, ",");
	char *prot = strtok(NULL, "\n");

	strftime(date, sizeof(date), "%Y%m%d", localtime(&unix_secs));

	if (ipcmp(srcaddr, networks)) {
	    snprintf(query, sizeof(query), "UPDATE `page1` SET `sent_bytes` = `sent_bytes` + %s, `sent_pkts` = `sent_pkts` + %s WHERE "
		"`date` = %s AND `host` = '%s'", doctets, dpkts, date, srcaddr);

	    mysql_query(&dbh, query);

	    if (!mysql_affected_rows(&dbh)) {
		snprintf(query, sizeof(query), "INSERT INTO `page1` VALUES (NULL, %s, '%s', %s, 0, %s, 0)", date, srcaddr, doctets, dpkts);

		mysql_query(&dbh, query);
	    }

	    snprintf(query, sizeof(query), "UPDATE `page2` SET `sent_bytes` = `sent_bytes` + %s, `sent_pkts` = `sent_pkts` + %s WHERE "
		"`date` = %s AND `host` = '%s' AND `remote_host` = '%s' AND `port` = %s AND `prot` = %s",
		doctets, dpkts, date, srcaddr, dstaddr, dstport, prot);

	    mysql_query(&dbh, query);

	    if (!mysql_affected_rows(&dbh)) {
		snprintf(query, sizeof(query), "INSERT INTO `page2` VALUES (NULL, %s, '%s', '%s', %s, 0, %s, 0, %s, %s)",
		    date, srcaddr, dstaddr, doctets, dpkts, dstport, prot);

		mysql_query(&dbh, query);
	    }
	}

	if (ipcmp(dstaddr, networks)) {
	    snprintf(query, sizeof(query), "UPDATE `page1` SET `recv_bytes` = `recv_bytes` + %s, `recv_pkts` = `recv_pkts` + %s WHERE "
		"`date` = %s AND `host` = '%s'", doctets, dpkts, date, dstaddr);

	    mysql_query(&dbh, query);

	    if (!mysql_affected_rows(&dbh)) {
		snprintf(query, sizeof(query), "INSERT INTO `page1` VALUES (NULL, %s, '%s', 0, %s, 0, %s)", date, dstaddr, doctets, dpkts);

		mysql_query(&dbh, query);
	    }

	    snprintf(query, sizeof(query), "UPDATE `page2` SET `recv_bytes` = `recv_bytes` + %s, `recv_pkts` = `recv_pkts` + %s WHERE "
		"`date` = %s AND `host` = '%s' AND `remote_host` = '%s' AND `port` = %s AND `prot` = %s",
		doctets, dpkts, date, dstaddr, srcaddr, srcport, prot);

	    mysql_query(&dbh, query);

	    if (!mysql_affected_rows(&dbh)) {
		snprintf(query, sizeof(query), "INSERT INTO `page2` VALUES (NULL, %s, '%s', '%s', 0, %s, 0, %s, %s, %s)",
		    date, dstaddr, srcaddr, doctets, dpkts, srcport, prot);

		mysql_query(&dbh, query);
	    }
	}
    }

    free(networks);
    pclose(flow);
    mysql_close(&dbh);
    closelog();

    return 0;
}
