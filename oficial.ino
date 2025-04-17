#include <WiFi.h>              // Librería para conexión WiFi
#include <WebSocketsClient.h>  // Librería para WebSocket como cliente
#include <ESP32Servo.h>        // Librería para servomotores
#include <ArduinoJson.h>       // Librería para manejar JSON
#include <time.h>              // Para manejar la hora actual
#include <map>
#include <vector>

// id del comedero
#define FEEDER_ID "67f57821c597ab31781c074d"  // ID único del comedero


// WiFi
const char* ssid = "INFINITUM9E8E";
const char* password = "Xt1Vt2Ji9c";
WebSocketsClient webSocket;

// informacion de websocket
const char* ip = "192.168.1.74";
const int port = 3000;

// Crear objeto JSON para manejar mensajes entrantes
DynamicJsonDocument doc(1024);

// Pines servos
const int servoPin1 = 26;
const int servoPin2 = 27;
Servo servo1;
Servo servo2;

// Pines sensores ultrasónicos (nivel de comida)
const int trigComida1 = 4;
const int echoComida1 = 5;
const int trigComida2 = 18;
const int echoComida2 = 19;

// Pines sensores de proximidad (detección de aves)
const int trigAve1 = 21;
const int echoAve1 = 22;
const int trigAve2 = 23;
const int echoAve2 = 25;

// Estructura para almacenar horarios por día y compuerta
std::map<String, std::map<int, std::vector<std::pair<String, String>>>> horariosPermitidos;

// Control de envío de estado
unsigned long ultimoEnvio = 0;
const unsigned long INTERVALO_ENVIO = 10000;  // cada 10 segundos

// Manejador de eventos del WebSocket
void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  String message = String((char*)payload);

  switch (type) {
    case WStype_CONNECTED:
      Serial.println("🟢 Conectado al servidor WebSocket");
      // Enviar mensaje de conexión
      {
        StaticJsonDocument<128> doc;
        doc["type"] = "esp_connect";
        doc["feederId"] = FEEDER_ID;

        String mensaje;
        serializeJson(doc, mensaje);
        webSocket.sendTXT(mensaje);
      }
      break;

    case WStype_TEXT:
      Serial.print("mensaje recibido: ");
      Serial.println(message);
      parseJson(message);
      analizarAcciones();
      break;

    case WStype_DISCONNECTED:
      Serial.println("🔌 Desconectado del WebSocket");
      break;
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  Serial.print("Conectando a WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  configTime(0, 0, "pool.ntp.org");

  Serial.println("\n📄 Conectado!");
  Serial.print("IP local: ");
  Serial.println(WiFi.localIP());

  // conexion a websocket
  webSocket.begin(ip, port, "/");
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);

  // Configurar sensores
  pinMode(trigComida1, OUTPUT);
  pinMode(echoComida1, INPUT);
  pinMode(trigComida2, OUTPUT);
  pinMode(echoComida2, INPUT);
  pinMode(trigAve1, OUTPUT);
  pinMode(echoAve1, INPUT);
  pinMode(trigAve2, OUTPUT);
  pinMode(echoAve2, INPUT);

  // Iniciar servos
  servo1.attach(servoPin1);
  servo2.attach(servoPin2);
  servo1.write(0);
  servo2.write(0);
}

String obtenerDiaSemana() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "unknown";

  char buffer[10];
  strftime(buffer, sizeof(buffer), "%A", &timeinfo);
  return String(buffer);
}

String obtenerHoraActual() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "00:00";

  char buffer[6];
  strftime(buffer, sizeof(buffer), "%H:%M", &timeinfo);
  return String(buffer);
}

bool puedeAbrir(String horaActual, String horaInicio, String horaFin) {
  return horaActual >= horaInicio && horaActual <= horaFin;
}

bool estaPermitido(String dia, String hora, int compuerta) {
  if (!horariosPermitidos.count(dia)) return false;
  if (!horariosPermitidos[dia].count(compuerta)) return false;

  for (auto& intervalo : horariosPermitidos[dia][compuerta]) {
    if (puedeAbrir(hora, intervalo.first, intervalo.second)) {
      return true;
    }
  }
  return false;
}

int calcularNivelComida(long distancia) {
  distancia = constrain(distancia, 1, 30);
  return map(distancia, 30, 1, 0, 100);
}

String crearMensajeEstado(int comida1, int comida2, int bateria) {
  StaticJsonDocument<256> doc;
  doc["type"] = "update_status";
  doc["feederId"] = FEEDER_ID;
  doc["batteryLevel"] = bateria;
  doc["isActive"] = true;

  JsonObject fg1 = doc["floodgates"].createNestedObject("1");
  fg1["foodLevel"] = comida1;
  JsonObject fg2 = doc["floodgates"].createNestedObject("2");
  fg2["foodLevel"] = comida2;

  String mensaje;
  serializeJson(doc, mensaje);
  return mensaje;
}

void manejarServo(Servo& serv, bool open) {
  serv.write(open ? 90 : 0);
}

void loop() {
  webSocket.loop();

  String dia = obtenerDiaSemana();
  String hora = obtenerHoraActual();

  long ave1 = medirDistancia(trigAve1, echoAve1);
  long ave2 = medirDistancia(trigAve2, echoAve2);

  if (ave1 >= 2 && ave1 <= 6 && estaPermitido(dia, hora, 1)) {
    manejarServo(servo1, true);
    webSocket.sendTXT("🗣️ Ave detectada - Servo1 abierto");
    delay(3000);
    manejarServo(servo1, false);
    webSocket.sendTXT("Servo1 cerrado después de alimentar");
  }

  if (ave2 >= 2 && ave2 <= 6 && estaPermitido(dia, hora, 2)) {
    manejarServo(servo2, true);
    webSocket.sendTXT("🗣️ Ave detectada - Servo2 abierto");
    delay(3000);
    manejarServo(servo2, false);
    webSocket.sendTXT("Servo2 cerrado después de alimentar");
  }

  if (millis() - ultimoEnvio >= INTERVALO_ENVIO) {
    long comida1 = medirDistancia(trigComida1, echoComida1);
    long comida2 = medirDistancia(trigComida2, echoComida2);
    int nivel1 = calcularNivelComida(comida1);
    int nivel2 = calcularNivelComida(comida2);
    int bateria = random(60, 100);  // Simulación de batería

    String msg = crearMensajeEstado(nivel1, nivel2, bateria);
    webSocket.sendTXT(msg);
    ultimoEnvio = millis();
  }

  delay(300);
}

// Parsear JSON entrante desde el backend
void parseJson(String message) {
  DeserializationError error = deserializeJson(doc, message);
  if (error) {
    Serial.println("Error al parsear el JSON");
    Serial.println(message);  // para depurar mejor
    return;
  }

  // Validar que tenga floodgates
  if (doc.containsKey("floodgates")) {
    horariosPermitidos.clear();

    JsonObject floodgates = doc["floodgates"];

    for (JsonPair fg : floodgates) {
      int compuerta = atoi(fg.key().c_str());
      JsonObject dias = fg.value().as<JsonObject>();

      // Recorremos cada día de la semana
      const char* diasSemana[] = {
        "monday", "tuesday", "wednesday", "thursday",
        "friday", "saturday", "sunday"
      };

      for (const char* dia : diasSemana) {
        if (!dias.containsKey(dia)) continue;

        JsonObject rango = dias[dia].as<JsonObject>();
        String inicio = rango["startTime"].as<String>();
        String fin = rango["endTime"].as<String>();

        String diaCapitalizado = String(dia);
        diaCapitalizado[0] = toupper(diaCapitalizado[0]);

        horariosPermitidos[diaCapitalizado][compuerta].push_back({ inicio, fin });
      }
    }

    Serial.println("🗓️ Horarios actualizados");

    for (auto& dia : horariosPermitidos) {
      Serial.println("📅 Día: " + dia.first);
      for (auto& compuerta : dia.second) {
        Serial.print("  🚪 Compuerta ");
        Serial.println(compuerta.first);
        for (auto& intervalo : compuerta.second) {
          Serial.print("    ⏰ Desde ");
          Serial.print(intervalo.first);
          Serial.print(" hasta ");
          Serial.println(intervalo.second);
        }
      }
    }
  }
}

// Analizar acciones del backend
void analizarAcciones() {
  const String action = doc["type"];

  if (action == "open_servos") {
    manejarServo(servo1, true);
    manejarServo(servo2, true);
    // webSocket.sendTXT("Servos abiertos");
    Serial.println("Servos abiertos");
    return;
  }

  if (action == "close_servos") {
    manejarServo(servo1, false);
    manejarServo(servo2, false);
    // webSocket.sendTXT("Servos cerrados");
    Serial.println("Servos cerrados");
    return;
  }

  if (action == "open_servo") {
    if (doc["servo_number"] == 1) {
      manejarServo(servo1, true);
      // webSocket.sendTXT("Servo1 abierto");
      Serial.println("Servo1 abierto");
    } else {
      manejarServo(servo2, true);
      // webSocket.sendTXT("Servo2 abierto");
      Serial.println("Servo2 abierto");
    }
  }

  if (action == "close_servo") {
    if (doc["servo_number"] == 1) {
      manejarServo(servo1, false);
      // webSocket.sendTXT("Servo1 cerrado");
      Serial.println("Servo1 cerrado");
    } else {
      manejarServo(servo2, false);
      // webSocket.sendTXT("Servo2 cerrado");
      Serial.println("Servo2 cerrado");
    }
  }
}

// Medir distancia en cm usando sensor ultrasónico
long medirDistancia(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duracion = pulseIn(echoPin, HIGH, 30000);
  long distancia = duracion * 0.034 / 2;
  return distancia;
}
