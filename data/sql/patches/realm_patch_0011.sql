CREATE TABLE `character_auras` (
  `guid` int(20) unsigned NOT NULL,
  `caster_guid` bigint(20) unsigned NOT NULL,
  `item_guid` bigint(20) unsigned NOT NULL,
  `spell` int(11) NOT NULL,
  `stack_count` int(10) unsigned DEFAULT NULL,
  `remain_charges` int(10) unsigned DEFAULT NULL,
  `basepoints_0` int(11) DEFAULT NULL,
  `basepoints_1` int(11) DEFAULT NULL,
  `basepoints_2` int(11) DEFAULT NULL,
  `periodictime_0` int(10) unsigned DEFAULT NULL,
  `periodictime_1` int(10) unsigned DEFAULT NULL,
  `periodictime_2` int(10) unsigned DEFAULT NULL,
  `max_duration` int(11) DEFAULT NULL,
  `remain_time` int(11) DEFAULT NULL,
  `eff_index_mask` int(10) unsigned DEFAULT NULL,
  PRIMARY KEY (`guid`),
  CONSTRAINT `character_auras_ibfk_1` FOREIGN KEY (`guid`) REFERENCES `character` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_german1_ci;
