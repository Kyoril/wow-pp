ALTER TABLE `character` ADD COLUMN `deleted_account` INT DEFAULT NULL;
ALTER TABLE `character` ADD COLUMN `deleted_timestamp` TIMESTAMP DEFAULT 0;