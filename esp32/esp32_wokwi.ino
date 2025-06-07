#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

// Pinos
#define TRIG_PIN     5
#define ECHO_PIN     18
#define DHT_PIN      4
#define DHT_TYPE     DHT22
#define BUTTON_PIN   2
#define BUZZER_PIN   13

// Objetos
DHT dht(DHT_PIN, DHT_TYPE);

// Variáveis
long duration;
float distance;
bool botaoPressionado = false;
const float nivelCritico = 100.0; // cm

// Wi-Fi
const char* ssid = "Wokwi-GUEST";
const char* password = "";
WiFiClient espClient;

// MQTT
const char* mqttServer = "broker.hivemq.com";
const int mqttPort = 1883;
const char* mqttTopic = "iot/projeto/nivelagua";
PubSubClient client(espClient);

void setup_wifi() {
  WiFi.begin(ssid, password);
  Serial.print("Conectando-se ao Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" conectado!");
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Conectando ao MQTT...");
    if (client.connect("ESP32Client")) {
      Serial.println(" conectado!");
    } else {
      Serial.print(" falha, rc=");
      Serial.print(client.state());
      Serial.println(" tentando novamente em 5s...");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  // Sensores e atuadores
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  dht.begin();

  // Conexões
  setup_wifi();
  client.setServer(mqttServer, mqttPort);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Sensor ultrassônico
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  duration = pulseIn(ECHO_PIN, HIGH, 30000); // Timeout de 30 ms
  if (duration == 0) {
    // Tentativa rápida extra se nada foi detectado
    duration = pulseIn(ECHO_PIN, HIGH, 30000);
  }
  distance = duration * 0.034 / 2;
  if (distance <= 0) {
    distance = -1; // Indicador de erro na leitura
  }

  // Sensor DHT22
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  // Validar leituras DHT
  if (isnan(humidity)) humidity = -1;
  if (isnan(temperature)) temperature = -1;

  // Botão
  botaoPressionado = digitalRead(BUTTON_PIN) == LOW;

  // Buzzer
  if (botaoPressionado || (distance > 0 && distance <= nivelCritico)) {
    digitalWrite(BUZZER_PIN, HIGH);
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }

  // Mostrar no Serial
  Serial.println("------ Dados IoT ------");
  Serial.printf("Distância: %.2f cm\n", distance);
  Serial.printf("Temperatura: %.2f °C\n", temperature);
  Serial.printf("Umidade: %.2f %%\n", humidity);
  Serial.printf("Botão: %s\n", botaoPressionado ? "ACIONADO" : "Não acionado");
  Serial.printf("Buzzer: %s\n", digitalRead(BUZZER_PIN) ? "LIGADO" : "DESLIGADO");
  Serial.println("------------------------\n");

  // Enviar via MQTT
  char payload[200];
  snprintf(payload, sizeof(payload),
    "{\"distancia\": %.2f, \"temperatura\": %.2f, \"umidade\": %.2f, \"botao\": %s}",
    distance, temperature, humidity, botaoPressionado ? "true" : "false");

  client.publish(mqttTopic, payload, false);  // QoS 0, retain false

  delay(2000);
}
