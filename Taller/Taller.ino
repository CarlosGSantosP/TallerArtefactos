#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

// ======================================================
// CONFIGURACIÓN WIFI
// ======================================================

const char* WIFI_SSID = "Wifi";
const char* WIFI_PASSWORD = "Maria123";

// ======================================================
// CONFIGURACIÓN MQTT
// ======================================================

const char* MQTT_SERVER = "10.189.52.6";
const int MQTT_PORT = 1883;

// Topics utilizados por el dashboard React.
const char* TOPIC_TEMPERATURA = "clima/esp32/temperatura";
const char* TOPIC_HUMEDAD = "clima/esp32/humedad";
const char* TOPIC_LUZ = "clima/esp32/luz";

// ======================================================
// CONFIGURACIÓN DE SENSORES
// ======================================================

#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define LDRPIN 32

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// Enviar una lectura cada 2 segundos.
const unsigned long INTERVALO_ENVIO = 2000;
unsigned long ultimoEnvio = 0;

void conectarWiFi() {
  Serial.print("Conectando a WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi conectado");
  Serial.print("IP del ESP32: ");
  Serial.println(WiFi.localIP());
}

void conectarMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Conectando a Mosquitto...");

    // Crear un Client ID diferente para este ESP32.
    String clientId = "ESP32-";
    clientId += String(
      (uint32_t)(ESP.getEfuseMac() & 0xFFFFFFFF),
      HEX
    );

    if (mqttClient.connect(clientId.c_str())) {
      Serial.println(" conectado");
    } else {
      Serial.print(" error, rc=");
      Serial.print(mqttClient.state());
      Serial.println(". Reintentando en 2 segundos...");
      delay(2000);
    }
  }
}

// ======================================================
// LEER Y PUBLICAR SENSORES
// ======================================================

void publicarSensores() {
  // 1. Obtener temperatura y humedad
  float temperatura = dht.readTemperature();
  float humedad = dht.readHumidity();

  // Verificar si la lectura del DHT fue válida
  if (isnan(temperatura) || isnan(humedad)) {
    Serial.println("Error al leer el sensor DHT11");
    return;
  }

  // 2. Obtener lectura analógica del LDR (ADC 12 bits: 0 a 4095) y convertir a porcentaje (0 a 100%)
  int lecturaLDR = analogRead(LDRPIN);
  int porcentajeLuz = map(lecturaLDR, 0, 4095, 0, 100);
  porcentajeLuz = constrain(porcentajeLuz, 0, 100);

  // 3. Convertir valores numéricos a cadenas para la transmisión MQTT
  char tempStr[8];
  char humStr[8];
  char luzStr[8];

  dtostrf(temperatura, 1, 1, tempStr);
  dtostrf(humedad, 1, 1, humStr);
  snprintf(luzStr, sizeof(luzStr), "%d", porcentajeLuz);

  // 4. Publicar datos con retain = true
  mqttClient.publish(TOPIC_TEMPERATURA, tempStr, true);
  mqttClient.publish(TOPIC_HUMEDAD, humStr, true);
  mqttClient.publish(TOPIC_LUZ, luzStr, true);

  // 5. Imprimir en el monitor serial
  Serial.println("-----------------------------");
  Serial.print("Temperatura: ");
  Serial.print(tempStr);
  Serial.println(" °C");

  Serial.print("Humedad: ");
  Serial.print(humStr);
  Serial.println(" %");

  Serial.print("Luz: ");
  Serial.print(luzStr);
  Serial.println(" %");
}

void setup() {
  Serial.begin(115200);

  // Inicializar sensores
  dht.begin();
  pinMode(LDRPIN, INPUT);

  conectarWiFi();

  mqttClient.setServer(
    MQTT_SERVER,
    MQTT_PORT
  );
}

void loop() {
  // Si se pierde WiFi, volver a conectarse.
  if (WiFi.status() != WL_CONNECTED) {
    conectarWiFi();
  }

  // Si se pierde MQTT, volver a conectarse.
  if (!mqttClient.connected()) {
    conectarMQTT();
  }

  mqttClient.loop();

  // Publicar sensores cada 2 segundos.
  if (millis() - ultimoEnvio >= INTERVALO_ENVIO) {
    ultimoEnvio = millis();
    publicarSensores();
  }
}