#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <WiFiMulti.h>
#include <WiFiClientSecure.h>
#include <WebSocketsServer.h>

// === Config WiFi ===
const char* ssid = "SSID";
const char* password = "PAS";

// === Config WebSocket ===
WiFiMulti WiFiMulti;
WebSocketsServer webSocket(81);
AsyncWebServer server(80);

// === Sensor UV ===
const int pinAnalogico = 34;
const float K_UVI_PER_V = 15.0f;   // Escala aprox: 1V ≈ 15 UVI

// === Intervalo de envío ===
const uint16_t dataTxTimeInterval = 1000;

// === Prototipo de función WebSocket ===
void onWebSocketEvent(uint8_t client_num, WStype_t type, uint8_t *payload, size_t length);

void setup() {
  Serial.begin(115200);

  // Conectar WiFi
  Serial.println("Conectando a la red WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nConectado");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // Config ADC
  analogReadResolution(12);
  analogSetPinAttenuation(pinAnalogico, ADC_0db);  // rango 0-1.1V

  // WebSocket
  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);

  // Servidor HTTP básico
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", "ESP32 UV Server corriendo");
  });
  server.begin();
}

void loop() {
  webSocket.loop();

  static uint32_t prevMillis = 0;
  if (millis() - prevMillis >= dataTxTimeInterval) {
    prevMillis = millis();

    // === Leer sensor ===
    int mv = analogReadMilliVolts(pinAnalogico);   // en mV calibrados
    float v = mv / 1000.0f;                        // a volt
    float uvi = v * K_UVI_PER_V;                   // UVI estimado
    if (uvi < 0) uvi = 0;
    float E_Wm2   = uvi * 0.025f;                  // W/m^2
    float E_mWcm2 = E_Wm2 * 0.1f;                  // mW/cm^2

    // === JSON data ===
   String data = "{";
  data += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  data += "\"voltaje\":" + String(v, 3) + ",";
  data += "\"uvi\":" + String(uvi, 2) + ",";
  data += "\"irradiancia_wm2\":" + String(E_Wm2, 3) + ",";
  data += "\"irradiancia_mwcm2\":" + String(E_mWcm2, 4);
  data += "}";

    // Enviar a todos los clientes
    webSocket.broadcastTXT(data);

    // Debug en serie
    Serial.println(data);
  }
}

// === Evento WebSocket ===
void onWebSocketEvent(uint8_t client_num, WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_TEXT:
      Serial.print("Mensaje recibido del cliente #");
      Serial.print(client_num);
      Serial.print(": ");
      Serial.println((char*)payload);
      break;
  }
}

