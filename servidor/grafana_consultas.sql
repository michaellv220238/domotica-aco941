-- Fuente de datos en Grafana: PostgreSQL · host localhost:5432 · BD domotica · usuario grafana_ro · TLS/SSL disable

-- Panel "Temperatura y humedad" (Time series)
SELECT ts AS "time", sensor AS metric, valor
FROM lecturas
WHERE sensor IN ('sala/temperatura','sala/humedad') AND $__timeFilter(ts)
ORDER BY ts;

-- Panel "Gas y luz" (Time series)
SELECT ts AS "time", sensor AS metric, valor
FROM lecturas
WHERE sensor IN ('cocina/gas','exterior/luz') AND $__timeFilter(ts)
ORDER BY ts;

-- Panel "Movimientos detectados" (Stat)
SELECT count(*) AS movimientos
FROM lecturas
WHERE sensor = 'pasillo/movimiento' AND valor = 1 AND $__timeFilter(ts);

-- Panel "Últimos eventos de actuadores" (Table)
SELECT ts AS "Hora", actuador AS "Actuador", estado AS "Estado"
FROM eventos
WHERE $__timeFilter(ts)
ORDER BY ts DESC
LIMIT 20;
