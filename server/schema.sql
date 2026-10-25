-- GRAB device-profile store (MySQL 5.7+ / MariaDB 10.2+ for the JSON columns;
-- swap JSON -> LONGTEXT if your host is older).
--
--   mysql -u <user> -p <dbname> < schema.sql

CREATE TABLE IF NOT EXISTS grab_profiles (
  id           BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
  received_at  DATETIME        NOT NULL,
  family       VARCHAR(16)     NOT NULL,
  type         VARCHAR(64)     NOT NULL,
  firmware     VARCHAR(32)     NOT NULL,
  transport    VARCHAR(8)      NOT NULL,
  serial_hash  VARCHAR(80)     NOT NULL,
  tool_name    VARCHAR(32)     NULL,
  tool_version VARCHAR(16)     NULL,
  notes        TEXT            NULL,
  raw_json     JSON            NOT NULL,
  INDEX idx_device (family, type),
  INDEX idx_serial (serial_hash, firmware)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Connection diagnostics (opt-in): which MIDI ports GRAB saw and how the connect
-- attempt ended. For diagnosing/adding device support (RISE/GRAND especially).
CREATE TABLE IF NOT EXISTS grab_diagnostics (
  id            BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
  received_at   DATETIME        NOT NULL,
  os            VARCHAR(64)     NULL,
  req_transport VARCHAR(8)      NULL,
  outcome       VARCHAR(24)     NULL,
  rise_grand    TINYINT(1)      NOT NULL DEFAULT 0,
  type          VARCHAR(64)     NULL,   -- device type if one connected
  tool_version  VARCHAR(16)     NULL,
  notes         TEXT            NULL,
  raw_json      JSON            NOT NULL,
  INDEX idx_diag (outcome, type)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS grab_config_observations (
  id         BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
  profile_id BIGINT UNSIGNED NOT NULL,
  family     VARCHAR(16)     NOT NULL,
  type       VARCHAR(64)     NOT NULL,
  config_id  INT             NOT NULL,
  name       VARCHAR(128)    NULL,
  value      INT             NULL,
  min_val    INT             NULL,
  max_val    INT             NULL,
  ctype      VARCHAR(16)     NULL,
  grp        VARCHAR(64)     NULL,
  options    JSON            NULL,
  FOREIGN KEY (profile_id) REFERENCES grab_profiles(id) ON DELETE CASCADE,
  INDEX idx_obs (family, type, config_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
