
CREATE TABLE `realm_characters` (
  `realm` INT(10) UNSIGNED NOT NULL COMMENT 'Realm ID',
  `account` INT(10) UNSIGNED NOT NULL COMMENT 'Account ID',
  `count` INT(10) UNSIGNED DEFAULT '0' COMMENT 'Number of characters',
  PRIMARY KEY (`realm`,`account`),
  KEY `account` (`account`),
  CONSTRAINT `realm_characters_ibfk_1` FOREIGN KEY (`realm`) REFERENCES `realm` (`id`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `realm_characters_ibfk_2` FOREIGN KEY (`account`) REFERENCES `account` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=INNODB DEFAULT CHARSET=latin1 COLLATE=latin1_german1_ci;
