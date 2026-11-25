/*
 * ACO941 · Proyecto Integrador · Fase 4
 * Sistema domótico IoT – ESP32 + MQTT
 * 4 sensores: DHT22 (temp/hum), PIR (movimiento), LDR (luz), MQ-2 (gas)
 * 4 actuadores: luz (LED/relé), cerradura (servo), ventilador (relé), alarma (buzzer)
 */
#include <WiFi.h>
#include <PubSubClient.h>
#include "DHTesp.h"
#include <ESP32Servo.h>

// ---------- Configuración ----------
const char* WIFI_SSID = "Wokwi-GUEST";       // en hardware real: su red WiFi
const char* WIFI_PASS = "";
const char* MQTT_HOST = "broker.hivemq.com";  // puenteado al Mosquitto del servidor (servidor/bridge.conf)
const int   MQTT_PORT = 1883;
const String BASE     = "udb-aco941/lv220238/casa/";

// ---------- Pines ----------
const int PIN_DHT    = 15;
const int PIN_PIR    = 27;
const int PIN_LDR    = 34;  // ADC1, solo entrada
const int PIN_GAS    = 35;  // ADC1, solo entrada
const int PIN_LUZ    = 26;
const int PIN_SERVO  = 18;
const int PIN_VENT   = 19;
const int PIN_BUZZER = 25;

// ---------- Umbrales de automatización ----------
const int   UMBRAL_GAS_PCT = 50;
const float UMBRAL_TEMP    = 30.0;
const int   UMBRAL_OSCURO  = 30;
const unsigned long LUZ_TIMEOUT       = 30000;
const unsigned long INTERVALO_LECTURA = 5000;

WiFiClient wifi;
PubSubClient mqtt(wifi);
DHTesp dht;
Servo cerradura;

bool modoAuto = true;
bool luz = false, vent = false, alarma = false, abierta = false;
bool luzPorAuto = false, ventPorAuto = false, alarmaPorAuto = false;
bool movPrev = false;
unsigned long tLectura = 0, tMovimiento = 0;

void pub(const String& t, const String& v, bool retain = true) {
  mqtt.publish((BASE + t).c_str(), v.c_str(), retain);
}

// ---------- Actuadores (publican su estado confirmado) ----------
void setLuz(bool on, bool porAuto = false) {
  luz = on; luzPorAuto = on && porAuto;
  digitalWrite(PIN_LUZ, on);
  pub("sala/luz/estado", on ? "ON" : "OFF");
}
void setVent(bool on, bool porAuto = false) {
  vent = on; ventPorAuto = on && porAuto;
  digitalWrite(PIN_VENT, on);
  pub("cocina/ventilador/estado", on ? "ON" : "OFF");
}
void setAlarma(bool on, bool porAuto = false) {
  alarma = on; alarmaPorAuto = on && porAuto;
  if (on) tone(PIN_BUZZER, 1000); else noTone(PIN_BUZZER);
  pub("alarma/estado", on ? "ON" : "OFF");
}
void setCerradura(bool abrir) {
  abierta = abrir;
  cerradura.write(abrir ? 90 : 0);
  pub("entrada/cerradura/estado", abrir ? "UNLOCK" : "LOCK");
}

// ---------- Comandos desde el dashboard ----------
void onMensaje(char* topic, byte* payload, unsigned int len) {
  String t = String(topic).substring(BASE.length());
  String v; for (unsigned int i = 0; i < len; i++) v += (char)payload[i];
  v.trim(); v.toUpperCase();
  Serial.printf("<- %s = %s\n", t.c_str(), v.c_str());

  if      (t == "sala/luz/set")             setLuz(v == "ON");
  else if (t == "cocina/ventilador/set")    setVent(v == "ON");
  else if (t == "alarma/set")               setAlarma(v == "ON");
  else if (t == "entrada/cerradura/set")    setCerradura(v == "UNLOCK");
  else if (t == "modo/set") {
    modoAuto = (v == "AUTO");
    pub("modo/estado", modoAuto ? "AUTO" : "MANUAL");
  }
}

void conectarWiFi() {
  Serial.print("WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(250); Serial.print("."); }
  Serial.println(" conectado");
}

void mantenerMqtt() {
  if (mqtt.connected()) { mqtt.loop(); return; }
  static unsigned long tIntento = 0;
  if (millis() - tIntento < 3000) return;
  tIntento = millis();
  String id = "esp32-aco941-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  String lwt = BASE + "dispositivo/estado";
  Serial.print("MQTT...");
  // Last Will: si el ESP32 se desconecta, el broker publica "offline"
  if (mqtt.connect(id.c_str(), lwt.c_str(), 1, true, "offline")) {
    Serial.println(" conectado");
    pub("dispositivo/estado", "online");
    mqtt.subscribe((BASE + "+/+/set").c_str());
    mqtt.subscribe((BASE + "+/set").c_str());
    pub("sala/luz/estado", luz ? "ON" : "OFF");
    pub("cocina/ventilador/estado", vent ? "ON" : "OFF");
    pub("alarma/estado", alarma ? "ON" : "OFF");
    pub("entrada/cerradura/estado", abierta ? "UNLOCK" : "LOCK");
    pub("modo/estado", modoAuto ? "AUTO" : "MANUAL");
  } else {
    Serial.printf(" fallo rc=%d\n", mqtt.state());
  }
}

int leerLuzPct() { return 100 - (analogRead(PIN_LDR) * 100 / 4095); } // en Wokwi: más voltaje = más oscuro

// ---------- Lecturas + automatizaciones ----------
void leerSensores() {
  TempAndHumidity th = dht.getTempAndHumidity();
  int luzPct = leerLuzPct();
  int gasPct = analogRead(PIN_GAS) * 100 / 4095;

  if (!isnan(th.temperature)) {
    pub("sala/temperatura", String(th.temperature, 1));
    pub("sala/humedad",     String(th.humidity, 1));
  }
  pub("exterior/luz", String(luzPct));
  pub("cocina/gas",   String(gasPct));
  Serial.printf("T=%.1f H=%.1f Luz=%d%% Gas=%d%%\n", th.temperature, th.humidity, luzPct, gasPct);

  if (!modoAuto) return;
  bool hayGas = gasPct >= UMBRAL_GAS_PCT;
  bool calor  = !isnan(th.temperature) && th.temperature > UMBRAL_TEMP;

  if (hayGas && !alarma)                setAlarma(true, true);
  if (!hayGas && alarmaPorAuto)         setAlarma(false);
  if ((hayGas || calor) && !vent)       setVent(true, true);
  if (!hayGas && !calor && ventPorAuto) setVent(false);
  if (movPrev && luzPct < UMBRAL_OSCURO && !luz) setLuz(true, true);
}

void revisarMovimiento() {
  bool mov = digitalRead(PIN_PIR);
  if (mov != movPrev) {
    movPrev = mov;
    pub("pasillo/movimiento", mov ? "1" : "0");
    if (mov && modoAuto && leerLuzPct() < UMBRAL_OSCURO && !luz) setLuz(true, true);
  }
  if (mov) tMovimiento = millis();
  if (luzPorAuto && millis() - tMovimiento > LUZ_TIMEOUT) setLuz(false);
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_PIR, INPUT);
  pinMode(PIN_LUZ, OUTPUT);
  pinMode(PIN_VENT, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  dht.setup(PIN_DHT, DHTesp::DHT22);
  cerradura.attach(PIN_SERVO);
  cerradura.write(0);
  conectarWiFi();
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMensaje);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) conectarWiFi();
  mantenerMqtt();
  revisarMovimiento();
  if (millis() - tLectura >= INTERVALO_LECTURA) {
    tLectura = millis();
    if (mqtt.connected()) leerSensores();
  }
}
