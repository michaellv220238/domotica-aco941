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

**Simulación en Wokwi:** [AGREGAR ENLACE DEL PROYECTO WOKWI]

## Arquitectura

```
[Sensores] --> ESP32 --WiFi/MQTT--> broker.hivemq.com <==bridge==> Mosquitto (servidor) --> Node-RED --> Dashboard (/ui)
[Actuadores] <--/                                                                              \--> PostgreSQL --> Grafana
```

Diagrama: `docs/arquitectura.svg`

Servidor de la Fase 3 (Ubuntu Server, **instalación nativa, sin Docker**):
DNS (bind9), servidor web, Node-RED, Grafana, MongoDB y PostgreSQL, más Mosquitto como broker MQTT.

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

1. **Simulación:** en wokwi.com crear un proyecto ESP32 y pegar `firmware/sketch.ino`,
   `firmware/diagram.json` y `firmware/libraries.txt` (pestaña Library Manager).
2. **Mosquitto (servidor):**
   ```bash
   sudo apt install -y mosquitto mosquitto-clients
   sudo cp servidor/bridge.conf /etc/mosquitto/conf.d/
   sudo systemctl restart mosquitto && systemctl status mosquitto
   ```
3. **PostgreSQL:** cambiar `CAMBIAR_CLAVE` y ejecutar
   `sudo -u postgres psql -f servidor/base_datos.sql`.
4. **Node-RED** (`http://IP-SERVIDOR:1880`): en *Manage palette* instalar
   `node-red-dashboard` y `node-red-contrib-postgresql`, luego *Import* → `node-red/flows.json`.
   Poner la contraseña real en el nodo de configuración `domotica` y hacer *Deploy*.
   El dashboard queda en `http://IP-SERVIDOR:1880/ui`.
5. **Grafana** (`http://IP-SERVIDOR:3000`): agregar la fuente de datos PostgreSQL
   (usuario `grafana_ro`) y crear los paneles con `servidor/grafana_consultas.sql`.
6. **Prueba:** mover los controles del DHT22 y del MQ-2 y activar el PIR en Wokwi; los
   cambios se ven en el dashboard. Desde el dashboard se accionan luz, ventilador, alarma y
   cerradura. Para ver los mensajes:
   ```bash
   mosquitto_sub -h localhost -t 'udb-aco941/lv220238/casa/#' -v
   ```

## Estructura del repositorio

```
firmware/    sketch.ino, diagram.json, libraries.txt
servidor/    bridge.conf, base_datos.sql, grafana_consultas.sql
node-red/    flows.json (dashboard + registro en PostgreSQL)
docs/        arquitectura.svg
evidencias/  capturas del avance
```

## Limitaciones y mejoras futuras

- Es una simulación; el broker público no tiene autenticación.
- Mejoras: usuario, contraseña y TLS en MQTT; un ESP32 por planta; avisos al celular.
