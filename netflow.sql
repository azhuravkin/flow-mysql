CREATE TABLE `page1` (
  `id` bigint(15) NOT NULL AUTO_INCREMENT,
  `date` date default NULL,
  `host` varchar(45) NOT NULL default '',
  `sent_bytes` int(11) unsigned NOT NULL default '0',
  `recv_bytes` int(11) unsigned NOT NULL default '0',
  `sent_pkts` int(11) unsigned NOT NULL default '0',
  `recv_pkts` int(11) unsigned NOT NULL default '0',
  PRIMARY KEY (`id`),
  KEY `date` (`date`),
  KEY `host` (`host`),
  KEY `sent_bytes` (`sent_bytes`),
  KEY `recv_bytes` (`recv_bytes`),
  KEY `sent_pkts` (`sent_pkts`),
  KEY `recv_pkts` (`recv_pkts`)
);

CREATE TABLE `page2` (
  `id` bigint(15) NOT NULL AUTO_INCREMENT,
  `date` date default NULL,
  `host` varchar(45) NOT NULL default '',
  `remote_host` varchar(45) NOT NULL default '',
  `sent_bytes` int(11) unsigned NOT NULL default '0',
  `recv_bytes` int(11) unsigned NOT NULL default '0',
  `sent_pkts` int(11) unsigned NOT NULL default '0',
  `recv_pkts` int(11) unsigned NOT NULL default '0',
  `port` smallint(5) unsigned NOT NULL default '0',
  `prot` tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY (`id`),
  KEY `date` (`date`),
  KEY `host` (`host`),
  KEY `remote_host` (`remote_host`),
  KEY `sent_bytes` (`sent_bytes`),
  KEY `recv_bytes` (`recv_bytes`),
  KEY `sent_pkts` (`sent_pkts`),
  KEY `recv_pkts` (`recv_pkts`),
  KEY `port` (`port`),
  KEY `prot` (`prot`)
);
