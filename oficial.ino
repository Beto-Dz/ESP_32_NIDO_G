mi funcion manejar servo funciona bien?:
#include <WiFi.h>              // Librería para conexión WiFi
#include <WebSocketsClient.h>  // Librería para WebSocket como cliente
#include <ESP32Servo.h>        // libreria para servomotores
#include <ArduinoJson.h>       // Incluye la librería ArduinoJson



// WiFi
const char* ssid = "INFINITUM9E8E";
const char* password = "Xt1Vt2Ji9c";
WebSocketsClient webSocket;

// Crear un objeto JSON para acceder al mensaje JSON recibido del socket
DynamicJsonDocument doc(1024);  // Reserva memoria para el JSON (ajustar tamaño según el JSON que recibas)

// Pines servos
const int servoPin1 = 26;
const int servoPin2 = 27;
Servo servo1;
Servo servo2;

// Pines sensores ultrasónicos
// Nivel de comida
const int trigComida1 = 4;
const int echoComida1 = 5;
const int trigComida2 = 18;
const int echoComida2 = 19;

// Detección de aves
const int trigAve1 = 21;
const int echoAve1 = 22;
const int trigAve2 = 23;
const int echoAve2 = 25;

// Manejador de eventos del WebSocket
void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  // transformar el payload en string
  String message = String((char*)payload);

  switch (type) {
    case WStype_CONNECTED:
      Serial.println("🟢 Conectado al servidor WebSocket");
      webSocket.sendTXT("Hola desde ESP32!");  // Primer mensaje al conectarse
      break;

    case WStype_TEXT:
      // Converting payload to String and passing to JSON parsing function
      parseJson(message);
      analizarAcciones();
      break;

    case WStype_DISCONNECTED:
      Serial.println("🔌 Desconectado del WebSocket");
      break;
  }
}

void setup() {
  Serial.begin(115200);        // Iniciar monitor serial
  WiFi.begin(ssid, password);  // Intentar conectar al WiFi

  Serial.print("Conectando a WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n✅ Conectado!");
  Serial.print("IP local: ");
  Serial.println(WiFi.localIP());

  // Conectarse al WebSocket del backend (reemplazar IP si cambia)
  webSocket.begin("192.168.1.74", 3000, "/");
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);  // Reintentar si se desconecta

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

void manejarServo(Servo& serv, bool open) {
  if (open) {
    serv.write(90);
  } else {
    serv.write(0);
  }
}


void loop() {
  webSocket.loop();  // Procesar eventos del WebSocket


  // Medir distancias en los sensores de aves
  long ave1 = medirDistancia(trigAve1, echoAve1);
  long ave2 = medirDistancia(trigAve2, echoAve2);

  // Comprobar si hay un objeto entre 2 cm y 6 cm en ambos sensores
  if (ave1 >= 2 && ave1 <= 6) {
    // Abrir servo1
    // servo1.write(90);
    manejarServo(servo1, true);
    webSocket.sendTXT("🕊️ Ave detectada - Servo1 abierto");
    delay(3000);  // Mantener abiertos por 3 segundos
    manejarServo(servo1, false);
    webSocket.sendTXT("Servo1 cerrado después de alimentar");
  }

  if ((ave2 >= 2 && ave2 <= 6)) {
    manejarServo(servo2, true);
    webSocket.sendTXT("🕊️ Ave detectada - Servo2 abierto");
    delay(3000);
    manejarServo(servo2, false);
    webSocket.sendTXT("Servo2 cerrado después de alimentar");
  }

  delay(500);  // Pequeño retraso para evitar lecturas muy rápidas

  // Leer sensores
  // long comida1 = medirDistancia(trigComida1, echoComida1);
  // long comida2 = medirDistancia(trigComida2, echoComida2);
}

// Función para parsear el JSON recibido
void parseJson(String message) {
  // Deserializa el mensaje JSON
  DeserializationError error = deserializeJson(doc, message);

  // Comprobamos si hubo un error en la deserialización
  if (error) {
    Serial.println("Error al parsear el JSON");
    return;
  }
}

void analizarAcciones() {
  // Ahora accedemos a los valores en el JSON
  const String action = doc["action"];  // Lee la clave "action"

  if (action == "open_servos") {
    manejarServo(servo1, true);
    manejarServo(servo2, true);
    // Serial.println("Servos abiertos");
    webSocket.sendTXT("Servos abiertos");

    return;
  } else if (action == "close_servos") {
    manejarServo(servo1, false);
    manejarServo(servo2, false);
    // Serial.println("Servos cerrados");
    webSocket.sendTXT("Servos cerrados");
    return;
  }

  if (action == "open_servo") {
    if (doc["servo_number"] == 1) {
      manejarServo(servo1, true);
      // Serial.println("Servo1 abierto");
      webSocket.sendTXT("Servo1 abierto");
    } else {
      manejarServo(servo2, true);
      // Serial.println("Servo2 abierto");
      webSocket.sendTXT("Servo2 abierto");
    }
  } else if (action == "close_servo") {
    if (doc["servo_number"] == 1) {
      manejarServo(servo1, false);
      // Serial.println("Servo1 cerrado");
      webSocket.sendTXT("Servo1 cerrado");
    } else {
      manejarServo(servo2, false);
      // Serial.println("Servo2 cerrado");
      webSocket.sendTXT("Servo2 cerrado");
    }
  }
}

// Función para medir distancia en cm con HC-SR04
long medirDistancia(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duracion = pulseIn(echoPin, HIGH, 30000);  // timeout: 30 ms
  long distancia = duracion * 0.034 / 2;
  return distancia;
}
