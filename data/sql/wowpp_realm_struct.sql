/*
SQLyog Community v12.12 (64 bit)
MySQL - 5.6.26 : Database - wowpp_realm_01
*********************************************************************
*/

/*!40101 SET NAMES utf8 */;

/*!40101 SET SQL_MODE=''*/;

/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;
/*Table structure for table `character` */

DROP TABLE IF EXISTS `character`;

CREATE TABLE `character` (
  `id` int(11) unsigned NOT NULL AUTO_INCREMENT COMMENT 'Character identifier',
  `account` int(11) unsigned NOT NULL COMMENT 'Account identifier',
  `name` varchar(32) COLLATE latin1_german1_ci NOT NULL COMMENT 'Character name',
  `race` smallint(5) unsigned NOT NULL DEFAULT '0' COMMENT 'Character race',
  `class` smallint(5) unsigned NOT NULL DEFAULT '0' COMMENT 'Character class',
  `gender` tinyint(3) unsigned NOT NULL DEFAULT '0' COMMENT 'Character gender',
  `level` int(10) unsigned NOT NULL DEFAULT '1' COMMENT 'Character level',
  `xp` bigint(20) unsigned NOT NULL DEFAULT '0' COMMENT 'Gained experience points so far',
  `gold` int(10) unsigned NOT NULL DEFAULT '0' COMMENT 'Character gold',
  `online` tinyint(3) unsigned NOT NULL DEFAULT '0' COMMENT 'Determines whether the character is online',
  `cinematic` tinyint(3) unsigned NOT NULL DEFAULT '0' COMMENT 'Determines whether the cinematic was played',
  `map` int(10) unsigned NOT NULL DEFAULT '0' COMMENT 'Character map location',
  `zone` int(10) unsigned NOT NULL DEFAULT '0' COMMENT 'Character zone id',
  `position_x` double NOT NULL DEFAULT '-9458.05' COMMENT 'Character location (x)',
  `position_y` double NOT NULL DEFAULT '47.8475' COMMENT 'Character location (y)',
  `position_z` double NOT NULL DEFAULT '56.6068' COMMENT 'Character location (z)',
  `orientation` double NOT NULL DEFAULT '0' COMMENT 'Character orientation (radians)',
  `bytes` int(10) unsigned NOT NULL DEFAULT '0',
  `bytes2` int(10) unsigned NOT NULL DEFAULT '0',
  `flags` int(10) unsigned NOT NULL DEFAULT '0',
  `at_login` int(11) unsigned NOT NULL,
  `home_map` int(10) unsigned NOT NULL DEFAULT '0',
  `home_x` double NOT NULL,
  `home_y` double NOT NULL,
  `home_z` double NOT NULL,
  `home_o` double NOT NULL,
  `explored_zones` longtext COLLATE latin1_german1_ci,
  `last_save` bigint(20),
  `last_group` bigint(20) unsigned DEFAULT '0' COMMENT 'Characters last group id.',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=6 DEFAULT CHARSET=latin1 COLLATE=latin1_german1_ci;

/*Table structure for table `character_actions` */

DROP TABLE IF EXISTS `character_actions`;

CREATE TABLE `character_actions` (
  `guid` int(10) unsigned NOT NULL,
  `button` tinyint(3) unsigned NOT NULL,
  `action` smallint(5) unsigned NOT NULL,
  `type` tinyint(3) unsigned NOT NULL,
  PRIMARY KEY (`guid`,`button`),
  CONSTRAINT `character_actions_ibfk_1` FOREIGN KEY (`guid`) REFERENCES `character` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_german1_ci;

/*Table structure for table `character_items` */

DROP TABLE IF EXISTS `character_items`;

CREATE TABLE `character_items` (
  `id` int(11) unsigned NOT NULL AUTO_INCREMENT COMMENT 'Unique id of this item on this realm.',
  `owner` int(11) unsigned NOT NULL COMMENT 'GUID of the character who owns this item.',
  `entry` int(10) unsigned NOT NULL COMMENT 'Entry of the item template.',
  `slot` smallint(5) unsigned NOT NULL COMMENT 'Slot of this item.',
  `creator` int(11) unsigned DEFAULT NULL COMMENT 'GUID of the creator of this item (may be 0 if no creator information available)',
  `count` smallint(5) unsigned NOT NULL DEFAULT '1' COMMENT 'Number of items',
  `durability` smallint(5) unsigned NOT NULL DEFAULT '0' COMMENT 'Item''s durability (if this item has any durability).',
  PRIMARY KEY (`id`),
  KEY `owner_field` (`owner`),
  KEY `creator_field` (`creator`),
  CONSTRAINT `character_items_ibfk_1` FOREIGN KEY (`owner`) REFERENCES `character` (`id`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `character_items_ibfk_2` FOREIGN KEY (`creator`) REFERENCES `character` (`id`) ON DELETE SET NULL ON UPDATE CASCADE
) ENGINE=InnoDB AUTO_INCREMENT=248 DEFAULT CHARSET=latin1 COLLATE=latin1_german1_ci;

/*Table structure for table `character_social` */

DROP TABLE IF EXISTS `character_social`;

CREATE TABLE `character_social` (
  `guid_1` bigint(20) unsigned NOT NULL,
  `guid_2` bigint(20) unsigned NOT NULL,
  `flags` tinyint(3) unsigned NOT NULL,
  `note` varchar(48) COLLATE latin1_german1_ci DEFAULT NULL,
  PRIMARY KEY (`guid_1`,`guid_2`,`flags`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_german1_ci;

/*Table structure for table `character_spells` */

DROP TABLE IF EXISTS `character_spells`;

CREATE TABLE `character_spells` (
  `guid` int(10) unsigned NOT NULL,
  `spell` int(10) unsigned NOT NULL,
  PRIMARY KEY (`guid`,`spell`),
  CONSTRAINT `character_spells_ibfk_1` FOREIGN KEY (`guid`) REFERENCES `character` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_german1_ci;

/*Table structure for table `group` */

DROP TABLE IF EXISTS `group`;

CREATE TABLE `group` (
  `id` bigint(20) unsigned NOT NULL,
  `leader` int(11) unsigned NOT NULL,
  `loot_method` smallint(5) unsigned NOT NULL DEFAULT '0',
  `loot_master` int(11) unsigned DEFAULT NULL,
  `loot_treshold` smallint(5) unsigned NOT NULL DEFAULT '2',
  `icon_1` bigint(20) unsigned DEFAULT NULL,
  `icon_2` bigint(20) unsigned DEFAULT NULL,
  `icon_3` bigint(20) unsigned DEFAULT NULL,
  `icon_4` bigint(20) unsigned DEFAULT NULL,
  `icon_5` bigint(20) unsigned DEFAULT NULL,
  `icon_6` bigint(20) unsigned DEFAULT NULL,
  `icon_7` bigint(20) unsigned DEFAULT NULL,
  `icon_8` bigint(20) unsigned DEFAULT NULL,
  `type` smallint(5) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  KEY `group_leader_key` (`leader`),
  KEY `group_loot_master_key` (`loot_master`),
  CONSTRAINT `group_ibfk_1` FOREIGN KEY (`leader`) REFERENCES `character` (`id`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `group_ibfk_2` FOREIGN KEY (`loot_master`) REFERENCES `character` (`id`) ON DELETE SET NULL ON UPDATE SET NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_german1_ci;

/*Table structure for table `group_members` */

DROP TABLE IF EXISTS `group_members`;

CREATE TABLE `group_members` (
  `group` bigint(20) unsigned NOT NULL,
  `guid` int(11) unsigned NOT NULL,
  `assistant` smallint(5) unsigned DEFAULT '0',
  KEY `guid` (`guid`),
  KEY `group_members_ibfk_1` (`group`),
  CONSTRAINT `group_members_ibfk_1` FOREIGN KEY (`group`) REFERENCES `group` (`id`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `group_members_ibfk_2` FOREIGN KEY (`guid`) REFERENCES `character` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_german1_ci;

DROP TABLE IF EXISTS `character_cooldowns`;

CREATE TABLE `character_cooldowns` (
  `id` int(10) unsigned NOT NULL,
  `spell` int(10) unsigned NOT NULL,
  `end` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`,`spell`),
  CONSTRAINT `character_cooldowns_ibfk_1` FOREIGN KEY (`id`) REFERENCES `character` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_german1_ci;

DROP TABLE IF EXISTS `character_quests`;

CREATE TABLE `character_quests` (
  `guid` int(11) unsigned NOT NULL DEFAULT '0',
  `quest` int(11) unsigned NOT NULL DEFAULT '0',
  `status` int(11) unsigned NOT NULL DEFAULT '0',
  `explored` tinyint(1) unsigned NOT NULL DEFAULT '0',
  `timer` bigint(20) unsigned NOT NULL DEFAULT '0',
  `unitcount1` int(11) unsigned NOT NULL DEFAULT '0',
  `unitcount2` int(11) unsigned NOT NULL DEFAULT '0',
  `unitcount3` int(11) unsigned NOT NULL DEFAULT '0',
  `unitcount4` int(11) unsigned NOT NULL DEFAULT '0',
  `objectcount1` int(11) unsigned NOT NULL DEFAULT '0',
  `objectcount2` int(11) unsigned NOT NULL DEFAULT '0',
  `objectcount3` int(11) unsigned NOT NULL DEFAULT '0',
  `objectcount4` int(11) unsigned NOT NULL DEFAULT '0',
  `itemcount1` int(11) unsigned NOT NULL DEFAULT '0',
  `itemcount2` int(11) unsigned NOT NULL DEFAULT '0',
  `itemcount3` int(11) unsigned NOT NULL DEFAULT '0',
  `itemcount4` int(11) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`guid`,`quest`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;
