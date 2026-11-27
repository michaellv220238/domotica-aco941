# Sistema domótico IoT · ACO941 · Fase 4

Proyecto Integrador de **Arquitectura de Computadoras (ACO941)**, Universidad Don Bosco.
Docente: Denis Alfredo Altuve.

| Integrante | Carnet |
|---|---|
| López Valle, Michael Alejandro | LV220238 |
| Martínez Gutiérrez, Ronald Alexander | MG223061 |
| Espinal Flores, Grace Andrea | EF252930 |
| Pineda Castro, Laura Sofía | PC230111 |
| Viana Rodríguez, Melissa Estefany | VR150473 |

Un ESP32 lee 4 sensores y controla 4 actuadores. Todo viaja por **MQTT** al servidor
Ubuntu de la Fase 3, donde **Node-RED** muestra el dashboard y guarda el historial en
**PostgreSQL**, que **Grafana** grafica.

**Simulación en Wokwi:** https://wokwi.com/projects/481692131730253825

## Arquitectura

```
[Sensores] --> ESP32 --WiFi/MQTT--> broker.hivemq.com <==bridge==> Mosquitto (servidor) --> Node-RED --> Dashboard (/ui)
[Actuadores] <--/                                                                              \--> PostgreSQL --> Grafana
```

Diagrama: `docs/arquitectura.svg`

El despliegue probado usa **Ubuntu Server 24.04 y Docker Compose** para Mosquitto, Node-RED, PostgreSQL y Grafana. El navegador accede a los servicios web mediante un túnel SSH.

## Hardware

| Tipo | Componente | Pin ESP32 | Función |
|---|---|---|---|
| Sensor | DHT22 | GPIO15 | Temperatura y humedad de la sala |
| Sensor | PIR | GPIO27 | Movimiento en el pasillo |
| Sensor | LDR (fotorresistencia) | GPIO34 (ADC1) | Nivel de luz exterior |
| Sensor | MQ-2 | GPIO35 (ADC1) | Gas en la cocina |
| Actuador | LED / relé | GPIO26 | Luz de la sala |
| Actuador | Servomotor | GPIO18 (PWM) | Cerradura de la entrada |
| Actuador | Módulo relé | GPIO19 | Ventilador de la cocina |
| Actuador | Buzzer | GPIO25 | Alarma sonora |

LDR y MQ-2 usan GPIO34/35 porque son entradas del ADC1, que funciona con el WiFi activo.

## Tópicos MQTT

Prefijo: `udb-aco941/lv220238/casa/`

| Tópico | Dirección | Valores |
|---|---|---|
| `sala/temperatura`, `sala/humedad` | ESP32 → servidor | número, cada 5 s (retain) |
| `exterior/luz`, `cocina/gas` | ESP32 → servidor | 0–100 % |
| `pasillo/movimiento` | ESP32 → servidor | 1 / 0 al cambiar |
| `sala/luz/set`, `cocina/ventilador/set`, `alarma/set` | servidor → ESP32 | ON / OFF |
| `entrada/cerradura/set` | servidor → ESP32 | LOCK / UNLOCK |
| `modo/set` | servidor → ESP32 | AUTO / MANUAL |
| `…/estado` | ESP32 → servidor | estado confirmado del actuador |
| `dispositivo/estado` | ESP32 → servidor | online / offline (Last Will) |

Se separan `/set` (orden) y `/estado` (confirmación) para que el dashboard muestre el
estado real del actuador y no solo la orden enviada. `retain` hace que un dashboard recién
abierto vea los últimos valores, y el Last Will avisa si el ESP32 se desconecta.

## Automatizaciones (modo AUTO)

- Movimiento con poca luz (< 30 %): se enciende la luz por 30 s.
- Gas ≥ 50 %: suena la alarma y se enciende el ventilador.
- Temperatura > 30 °C: se enciende el ventilador.

Las reglas corren en el ESP32, así la casa sigue respondiendo aunque falle el servidor.

## Cómo ejecutarlo

### 1. Levantar los servicios en Ubuntu

Requisitos: Ubuntu Server 24.04, Docker Engine y Docker Compose. Desde la
raíz del repositorio clonado en la VM:

```bash
cd compose-lab
(umask 077; set -o noclobber; printf 'PG_PASSWORD=%s\n' "$(openssl rand -hex 24)" > .env)
sudo install -d -o 1000 -g 1000 data/nodered
sudo install -d -o 472 -g 0 data/grafana
sudo docker compose run --rm --no-deps -T --entrypoint npm nodered install --prefix /data --no-audit --no-fund node-red-dashboard@3.6.6 node-red-contrib-postgresql@0.16.2
sudo docker compose up -d
sudo docker compose ps
```

Creen el archivo `.env` una sola vez por instalación. No lo regeneren con una
base de datos ya inicializada. La primera vez, PostgreSQL ejecuta
`servidor/base_datos.sql` mediante `compose-lab/init-db.sh` y crea los usuarios
nodered y `grafana_ro`. Los datos y las credenciales locales se guardan fuera
del control de versiones.

### 2. Abrir el dashboard y Grafana

La VM de la demo usa NAT con el puerto SSH 2222 del anfitrión reenviado al
22 de Ubuntu. Los puertos web quedan accesibles por un túnel SSH. Ejecuten
este comando en el anfitrión con el usuario de su VM:

```bash
ssh -N -L 1880:127.0.0.1:1880 -L 3000:127.0.0.1:3000 -p 2222 user@127.0.0.1
```

- [Editor de Node-RED](http://127.0.0.1:1880/)
- [Dashboard de domótica](http://127.0.0.1:1880/ui)
- [Grafana](http://127.0.0.1:3000/)

En el editor de Node-RED, importen `node-red/flows.json` desde el menú
**Importar**. Ajusten estas dos conexiones compartidas e instancien el flujo:

| Conexión | Configuración |
|---|---|
| MQTT | Host mosquitto, puerto 1883 |
| PostgreSQL | Host `postgres`, puerto `5432`, base `domotica`, usuario `nodered` y la clave `PG_PASSWORD` del archivo `.env` |

En Grafana, cambien la contraseña de administrador en el primer inicio.
Agreguen una fuente PostgreSQL con host `postgres:5432`, base domotica,
usuario `grafana_ro` y la clave `PG_PASSWORD`. Para la conexión interna de Compose,
seleccionen **TLS/SSL Mode → disable**. Usen las consultas de
`servidor/grafana_consultas.sql` para crear los paneles.

### 3. Probar la simulación

1. Abran el proyecto de Wokwi. Para recrearlo, usen los tres archivos de
   firmware/ en un proyecto ESP32. Mantengan Wokwi y el dashboard visibles
   en ventanas separadas durante la prueba.
2. Cambien la temperatura del DHT22 y comprueben que el nuevo valor aparezca
   en el dashboard y en Grafana. Para verificar el registro en PostgreSQL:

   ```bash
   sudo docker compose exec postgres psql -U postgres -d domotica -c "SELECT ts, valor FROM lecturas WHERE sensor = 'sala/temperatura' ORDER BY ts DESC LIMIT 5;"
   ```

3. Cambien a modo manual y usen el dashboard para encender y apagar la luz.
   Verifiquen la respuesta del LED en Wokwi. Repitan la prueba para el
   ventilador, la alarma y la cerradura.
4. Para revisar los mensajes en la VM:

   ```bash
   sudo docker compose exec mosquitto mosquitto_sub -h 127.0.0.1 -t 'udb-aco941/lv220238/casa/#' -v -W 20
   ```

Si hay más de una copia del servidor, usen un `remote_clientid` distinto por
copia en `compose-lab/mosquitto.conf`. Para ensayar con el proyecto compartido
usen una sola simulación activa a la vez.

## Estructura del repositorio

```
firmware/    sketch.ino, diagram.json, libraries.txt
servidor/    configuración de referencia y SQL
compose-lab/ configuración del despliegue en la VM
node-red/    flows.json (dashboard + registro en PostgreSQL)
docs/        arquitectura.svg
evidencias/  capturas del avance
```

## Limitaciones y mejoras futuras

- Es una simulación con un broker MQTT público sin autenticación.
- Nginx, DNS con BIND9 y MongoDB quedan pendientes de integrar en el despliegue de la VM por tiempo nose pudo implementar .


