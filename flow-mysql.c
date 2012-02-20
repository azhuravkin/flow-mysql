#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <mysql/mysql.h>
#include <time.h>
#include <sys/stat.h>
#include <net/route.h>
#include <arpa/inet.h>

struct net {
    char str[16];
    int len;
};

struct arp {
    char ip[16];
    char mac[18];
    char dev[16];
};

struct route {
    uint32_t net;
    uint32_t mask;
    int metric;
    char dev[16];
};

struct arp *arp_list = NULL;
int arp_num = 0;

static int route_cmp(const void *p1, const void *p2) {
    struct route *r1 = (struct route *) p1;
    struct route *r2 = (struct route *) p2;

    if (r1->mask < r2->mask)
	return 1;
    else if (r1->mask > r2->mask)
	return -1;
    if (r1->metric > r2->metric)
	return 1;
    else if (r1->metric < r2->metric)
	return -1;
    return 0;
}

static int update_routes(struct route **routes) {
    FILE *fp;
    char buff[256];
    int num = 0;

    if ((fp = fopen("/proc/net/route", "r"))) {
	while (fgets(buff, sizeof(buff), fp)) {
	    char dev[16];
	    uint32_t net;
	    uint32_t mask;
	    int iflags, metric;
	
	    if ((sscanf(buff, "%16s %X %*128s %X %*d %*d %d %X %*d %*d %*d\n",
		    dev, &net, &iflags, &metric, &mask) == 5) && (iflags & RTF_UP)) {
		*routes = realloc(*routes, sizeof(struct route) * (num + 1));
		(*routes)[num].net = net;
		(*routes)[num].mask = mask;
		(*routes)[num].metric = metric;
		strcpy((*routes)[num].dev, dev);
		num++;
	    }
	}
	fclose(fp);
    }

    qsort(*routes, num, sizeof(struct route), route_cmp);

    return num;
}

static int ip_cmp(char *ip, struct net *network) {
    int i;

    for (i = 0; network[i].len; i++) {
	if (!strncmp(ip, network[i].str, network[i].len)) {
	    return i + 1;
	}
    }

    return 0;
}

static char *get_mac(char *ip) {
    int i;

    for (i = 0; i < arp_num; i++) {
	if (!strcmp(arp_list[i].ip, ip))
	    return arp_list[i].mac;
    }

    return "00:00:00:00:00:00";
}

static int update_cache(char *arpcache) {
    FILE *fp;
    char buffer[128];
    int num = 0;
    int i;
    struct stat st;

    if ((fp = fopen(arpcache, "rb"))) {
    	stat(arpcache, &st);
	num = st.st_size / sizeof(struct arp);
	arp_list = malloc(sizeof(struct arp) * num);
	fread(arp_list, sizeof(struct arp), num, fp);
	fclose(fp);
    }
    
    if ((fp = fopen("/proc/net/arp", "r"))) {
	while (fgets(buffer, sizeof(buffer), fp)) {
	    char ip[16];
	    char mac[18];
	    char dev[16];
	    int found = 0;

	    if (sscanf(buffer, "%16s %*x %*x %18s %*s %16s\n", ip, mac, dev) != 3)
		continue;

	    if (!strcmp(mac, "00:00:00:00:00:00"))
		continue;

	    for (i = 0; i < num; i++) {
		if (!strcmp(arp_list[i].mac, mac)) {
		    snprintf(arp_list[i].ip, sizeof(ip), "%s", ip); 
		    snprintf(arp_list[i].dev, sizeof(dev), "%s", dev);
		    found = 1;
		    break;
		}
	    }
	    
	    if (!found) {
		arp_list = realloc(arp_list, sizeof(struct arp) * (num + 1));
		memset(&arp_list[num], 0, sizeof(struct arp));
		snprintf(arp_list[num].ip, sizeof(arp_list[num].ip), "%s", ip);
		snprintf(arp_list[num].mac, sizeof(arp_list[num].mac), "%s", mac);
		snprintf(arp_list[num].dev, sizeof(arp_list[num].dev), "%s", dev);
		num++;
	    }
	}
	fclose(fp);
    }
    
    if ((fp = fopen(arpcache, "wb"))) {
	fwrite(arp_list, sizeof(struct arp), num, fp);
	fclose(fp);
    }

    return num;
}

static char *get_intf(struct route *routes, int num, char *ip) {
    struct sockaddr_in addr;
    int i;

    inet_aton(ip, &addr.sin_addr);

    for (i = 0; i < num; i++) {
	if ((addr.sin_addr.s_addr & routes[i].mask) == routes[i].net) {
	    return routes[i].dev;
	}
    }

    return "-";
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
    char arpcache[128];
    int port;
    char val[64];
    struct net *networks = NULL;
    struct route *routes = NULL;
    int routes_num;
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
    strcpy(arpcache, "/var/cache/arp.dat");
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
	    } else if ((sscanf(buf, "arpcache: %127[A-Za-z0-9 .:/_+-]", val)) == 1) {
		strcpy(arpcache, val);
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

    arp_num = update_cache(arpcache);
    routes_num = update_routes(&routes);

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
	int group_id;

	strftime(date, sizeof(date), "%Y%m%d", localtime(&unix_secs));

	if ((group_id = ip_cmp(srcaddr, networks))) {
	    snprintf(query, sizeof(query), "UPDATE `page1` SET `sent_bytes` = `sent_bytes` + %s, `sent_pkts` = `sent_pkts` + %s WHERE "
		"`date` = '%s' AND `group_id` = '%d' AND `ip` = '%s' AND `mac` = '%s' AND `intf` = '%s'",
		doctets, dpkts, date, group_id, srcaddr, get_mac(srcaddr), get_intf(routes, routes_num, dstaddr));

	    mysql_query(&dbh, query);

	    if (!mysql_affected_rows(&dbh)) {
		snprintf(query, sizeof(query), "INSERT INTO `page1` VALUES (NULL, %s, '%d', '%s', '%s', %s, 0, %s, 0, '%s')",
		    date, group_id, srcaddr, get_mac(srcaddr), doctets, dpkts, get_intf(routes, routes_num, dstaddr));

		mysql_query(&dbh, query);
	    }

	    snprintf(query, sizeof(query), "UPDATE `page2` SET `sent_bytes` = `sent_bytes` + %s, `sent_pkts` = `sent_pkts` + %s WHERE "
		"`date` = %s AND `ip` = '%s' AND `mac` = '%s' AND `remote_ip` = '%s' AND `port` = %s AND `prot` = %s AND `intf` = '%s'",
		doctets, dpkts, date, srcaddr, get_mac(srcaddr), dstaddr, dstport, prot, get_intf(routes, routes_num, dstaddr));

	    mysql_query(&dbh, query);

	    if (!mysql_affected_rows(&dbh)) {
		snprintf(query, sizeof(query), "INSERT INTO `page2` VALUES (NULL, %s, '%s', '%s', '%s', %s, 0, %s, 0, %s, %s, '%s')",
		    date, srcaddr, get_mac(srcaddr), dstaddr, doctets, dpkts, dstport, prot, get_intf(routes, routes_num, dstaddr));

		mysql_query(&dbh, query);
	    }
	}

	if ((group_id = ip_cmp(dstaddr, networks))) {
	    snprintf(query, sizeof(query), "UPDATE `page1` SET `recv_bytes` = `recv_bytes` + %s, `recv_pkts` = `recv_pkts` + %s WHERE "
		"`date` = %s AND `group_id` = '%d' AND`ip` = '%s' AND `mac` = '%s' AND `intf` = '%s'",
		doctets, dpkts, date, group_id, dstaddr, get_mac(dstaddr), get_intf(routes, routes_num, srcaddr));

	    mysql_query(&dbh, query);

	    if (!mysql_affected_rows(&dbh)) {
		snprintf(query, sizeof(query), "INSERT INTO `page1` VALUES (NULL, %s, '%d', '%s', '%s', 0, %s, 0, %s, '%s')",
		    date, group_id, dstaddr, get_mac(dstaddr), doctets, dpkts, get_intf(routes, routes_num, srcaddr));

		mysql_query(&dbh, query);
	    }

	    snprintf(query, sizeof(query), "UPDATE `page2` SET `recv_bytes` = `recv_bytes` + %s, `recv_pkts` = `recv_pkts` + %s WHERE "
		"`date` = %s AND `ip` = '%s' AND `mac` = '%s' AND `remote_ip` = '%s' AND `port` = %s AND `prot` = %s AND `intf` = '%s'",
		doctets, dpkts, date, dstaddr, get_mac(dstaddr), srcaddr, srcport, prot, get_intf(routes, routes_num, srcaddr));

	    mysql_query(&dbh, query);

	    if (!mysql_affected_rows(&dbh)) {
		snprintf(query, sizeof(query), "INSERT INTO `page2` VALUES (NULL, %s, '%s', '%s', '%s', 0, %s, 0, %s, %s, %s, '%s')",
		    date, dstaddr, get_mac(dstaddr), srcaddr, doctets, dpkts, srcport, prot, get_intf(routes, routes_num, srcaddr));

		mysql_query(&dbh, query);
	    }
	}
    }

    if (networks)
	free(networks);
    if (routes)
	free(routes);
    pclose(flow);
    mysql_close(&dbh);
    closelog();

    return 0;
}
