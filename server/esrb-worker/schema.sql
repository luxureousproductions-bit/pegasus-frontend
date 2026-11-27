-- Pegasus X ESRB descriptors worker: its D1 database.
-- Paste all of this into the database's Console once (Cloudflare dashboard >
-- Storage & Databases > D1 > pegasusx-esrb > Console) and run it. Running it
-- again is harmless.

-- One IGDB game's ESRB rating, as IGDB had it when it was looked up.
-- esrb NULL: IGDB has no ESRB rating for it ("checked, none found").
CREATE TABLE IF NOT EXISTS games (
    igdb_id      INTEGER PRIMARY KEY,
    name         TEXT,
    esrb         TEXT,
    descriptors  TEXT NOT NULL DEFAULT '[]',   -- JSON list: ESRB content descriptors
    interactive  TEXT NOT NULL DEFAULT '[]',   -- JSON list: ESRB interactive elements IGDB marks
    checked_at   INTEGER NOT NULL              -- unix seconds
);

-- A name + platform + year lookup and what it found.
-- igdb_id NULL: no game, or more than one ("checked, none found": never guessed).
CREATE TABLE IF NOT EXISTS lookups (
    lookup_key   TEXT PRIMARY KEY,             -- "name|platform id|year", the name lowercased
    igdb_id      INTEGER,
    reason       TEXT NOT NULL,                -- "found", "none", "several"
    checked_at   INTEGER NOT NULL
);

-- The Twitch app access token, shared by every copy of the worker.
CREATE TABLE IF NOT EXISTS token (
    id           INTEGER PRIMARY KEY CHECK (id = 1),
    access_token TEXT NOT NULL,
    expires_at   INTEGER NOT NULL              -- unix seconds
);

-- IGDB's limits, kept across every copy of the worker: requests started per
-- second (4 at most), and requests open now (8 at most).
CREATE TABLE IF NOT EXISTS igdb_seconds (
    second       INTEGER PRIMARY KEY,
    n            INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS igdb_open (
    id           TEXT PRIMARY KEY,
    started      INTEGER NOT NULL              -- unix milliseconds
);

-- Requests per address per minute.
CREATE TABLE IF NOT EXISTS ip_minutes (
    ip           TEXT NOT NULL,
    minute       INTEGER NOT NULL,
    n            INTEGER NOT NULL,
    PRIMARY KEY (ip, minute)
);
