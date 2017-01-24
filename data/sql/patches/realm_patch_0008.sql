CREATE TABLE `character_skills` (
  `guid` INT(10) UNSIGNED NOT NULL,
  `skill` INT(10) UNSIGNED NOT NULL,
  `current` INT(10) UNSIGNED NOT NULL DEFAULT '1',
  `max` INT(10) UNSIGNED NOT NULL DEFAULT '1',
  PRIMARY KEY (`guid`,`skill`),
  CONSTRAINT `character_skill_binding` FOREIGN KEY (`guid`) REFERENCES `character` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
);