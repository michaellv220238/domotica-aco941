#!/bin/sh
set -eu
case "$POSTGRES_PASSWORD" in
  ''|*[!a-f0-9]*) echo 'Se requiere una clave hexadecimal' >&2; return 1 ;;
esac
sed "s/CAMBIAR_CLAVE/$POSTGRES_PASSWORD/g" /project/base_datos.sql |
  psql -v ON_ERROR_STOP=1 --username "$POSTGRES_USER" --dbname postgres
