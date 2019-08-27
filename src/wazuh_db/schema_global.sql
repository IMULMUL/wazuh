/*
 * SQL Schema for global database
 * Copyright (C) 2015-2019, Wazuh Inc.
 * June 30, 2016.
 * This program is a free software, you can redistribute it
 * and/or modify it under the terms of GPLv2.
 */

CREATE TABLE IF NOT EXISTS agent (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    ip TEXT,
    register_ip TEXT,
    internal_key TEXT,
    os_name TEXT,
    os_version TEXT,
    os_major TEXT,
    os_minor TEXT,
    os_codename TEXT,
    os_build TEXT,
    os_platform TEXT,
    os_uname TEXT,
    os_arch TEXT,
    version TEXT,
    config_sum TEXT,
    merged_sum TEXT,
    manager_host TEXT,
    node_name TEXT DEFAULT 'unknown',
    date_add TEXT NOT NULL,
    last_keepalive TEXT,
    status TEXT NOT NULL CHECK (status IN ('empty', 'pending', 'updated')) DEFAULT 'empty',
    fim_offset INTEGER NOT NULL DEFAULT 0,
    reg_offset INTEGER NOT NULL DEFAULT 0,
    `group` TEXT DEFAULT 'default'
);

CREATE INDEX IF NOT EXISTS agent_name ON agent (name);
CREATE INDEX IF NOT EXISTS agent_ip ON agent (ip);

INSERT INTO agent (id, ip, register_ip, name, date_add, last_keepalive, `group`) VALUES (0, '127.0.0.1', '127.0.0.1', 'localhost', datetime(CURRENT_TIMESTAMP, 'localtime'), '9999-12-31 23:59:59', NULL);

CREATE TABLE IF NOT EXISTS info (
    key TEXT PRIMARY KEY,
    value TEXT
);

CREATE TABLE IF NOT EXISTS `group`
    (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT
    );

CREATE TABLE IF NOT EXISTS belongs
    (
    id_agent INTEGER,
    id_group INTEGER,
    PRIMARY KEY (id_agent, id_group)
    );

CREATE TABLE IF NOT EXISTS attack
    (
    id TEXT PRIMARY KEY, 
    json TEXT
    );

CREATE TABLE IF NOT EXISTS has_phase
    (
    attack_id TEXT, 
    phase_name TEXT,
    FOREIGN KEY(attack_id) REFERENCES attack(id)
    );

CREATE TABLE IF NOT EXISTS has_platform
    (
    attack_id TEXT, 
    platform_name TEXT,
    FOREIGN KEY(attack_id) REFERENCES attack(id)
    );

PRAGMA journal_mode=WAL;
