/*
SQLyog Community v12.02 (64 bit)
MySQL - 5.5.27 : Database - wowpp_login
*********************************************************************
*/

/*!40101 SET NAMES utf8 */;

/*!40101 SET SQL_MODE=''*/;

/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;
USE `wowpp_login`;

/*Table structure for table `account` */

DROP TABLE IF EXISTS `account`;

CREATE TABLE `account` (
  `id` int(11) unsigned NOT NULL AUTO_INCREMENT COMMENT 'Account identifier',
  `username` varchar(32) COLLATE latin1_german1_ci NOT NULL COMMENT 'Account name in uppercase letters',
  `password` varchar(40) COLLATE latin1_german1_ci NOT NULL COMMENT 'Password hash sha1 encrypted',
  `v` longtext COLLATE latin1_german1_ci,
  `s` longtext COLLATE latin1_german1_ci,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=3 DEFAULT CHARSET=latin1 COLLATE=latin1_german1_ci;

/*Table structure for table `realm` */

DROP TABLE IF EXISTS `realm`;

CREATE TABLE `realm` (
  `id` int(11) unsigned NOT NULL AUTO_INCREMENT COMMENT 'Realm identifier',
  `internalName` varchar(32) COLLATE latin1_german1_ci NOT NULL COMMENT 'Internal name for login',
  `password` varchar(32) COLLATE latin1_german1_ci NOT NULL DEFAULT 'none' COMMENT 'Password for login',
  `lastVisibleName` varchar(32) COLLATE latin1_german1_ci NOT NULL DEFAULT 'Unnamed' COMMENT 'Last used visible name',
  `lastHost` varchar(32) COLLATE latin1_german1_ci NOT NULL DEFAULT '127.0.0.1' COMMENT 'Last used player host',
  `lastPort` smallint(5) unsigned NOT NULL DEFAULT '8129' COMMENT 'Last used player port',
  `online` tinyint(1) NOT NULL DEFAULT '0' COMMENT 'If the realm is currently online',
  `players` int(11) unsigned NOT NULL DEFAULT '0' COMMENT 'Number of players online',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=3 DEFAULT CHARSET=latin1 COLLATE=latin1_german1_ci;

/*Data for the table `realm` */

insert  into `realm`(`id`,`internalName`,`password`,`lastVisibleName`,`lastHost`,`lastPort`,`online`,`players`) values (1,'realm_01','none','Unnamed','127.0.0.1',8129,0,0);

DROP TABLE IF EXISTS `account_tutorials`;

CREATE TABLE `account_tutorials` (
  `account` int(11) unsigned NOT NULL,
  `tutorial_0` int(11) unsigned DEFAULT '0',
  `tutorial_1` int(11) unsigned DEFAULT '0',
  `tutorial_2` int(11) unsigned DEFAULT '0',
  `tutorial_3` int(11) unsigned DEFAULT '0',
  `tutorial_4` int(11) unsigned DEFAULT '0',
  `tutorial_5` int(11) unsigned DEFAULT '0',
  `tutorial_6` int(11) unsigned DEFAULT '0',
  `tutorial_7` int(11) unsigned DEFAULT '0',
  PRIMARY KEY (`account`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_german1_ci;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;
