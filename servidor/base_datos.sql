-- Ejecutar en el servidor:  sudo -u postgres psql -f base_datos.sql
-- Cambien CAMBIAR_CLAVE por contraseñas reales (no las suban a GitHub).

CREATE USER nodered    WITH PASSWORD 'CAMBIAR_CLAVE';
CREATE USER grafana_ro WITH PASSWORD 'CAMBIAR_CLAVE';
CREATE DATABASE domotica OWNER nodered;

\c domotica

CREATE TABLE lecturas (
  id     BIGSERIAL PRIMARY KEY,
  ts     TIMESTAMPTZ NOT NULL DEFAULT now(),
  sensor TEXT NOT NULL,             -- ej. 'sala/temperatura'
  valor  DOUBLE PRECISION NOT NULL
);
CREATE INDEX idx_lecturas_sensor_ts ON lecturas (sensor, ts DESC);

CREATE TABLE eventos (
  id       BIGSERIAL PRIMARY KEY,
  ts       TIMESTAMPTZ NOT NULL DEFAULT now(),
  actuador TEXT NOT NULL,           -- ej. 'sala/luz'
  estado   TEXT NOT NULL            -- ON/OFF, LOCK/UNLOCK
);
CREATE INDEX idx_eventos_ts ON eventos (ts DESC);

ALTER TABLE lecturas OWNER TO nodered;
ALTER TABLE eventos  OWNER TO nodered;

-- Usuario de solo lectura para Grafana
GRANT CONNECT ON DATABASE domotica TO grafana_ro;
GRANT USAGE ON SCHEMA public TO grafana_ro;
GRANT SELECT ON lecturas, eventos TO grafana_ro;
