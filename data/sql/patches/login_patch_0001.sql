DROP TABLE IF EXISTS `team_server`;

CREATE TABLE `team_server` (
  `id` int(11) unsigned NOT NULL AUTO_INCREMENT COMMENT 'Team server identifier',
  `internalName` varchar(32) COLLATE latin1_german1_ci NOT NULL COMMENT 'Internal name for login',
  `password` varchar(32) COLLATE latin1_german1_ci NOT NULL DEFAULT 'none' COMMENT 'Password for login',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=3 DEFAULT CHARSET=latin1 COLLATE=latin1_german1_ci;