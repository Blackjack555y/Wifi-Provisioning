/**
 * @file esp32_provisioning.ino
 * @brief ESP32 WiFi Provisioning via Captive Portal
 *
 * Permite configurar credenciales WiFi sin reprogramar el dispositivo.
 * - Modo AP con portal cautivo si no hay credenciales guardadas
 * - Almacenamiento en NVS (Preferences)
 * - Reconexión automática
 * - Reset de configuración por botón o endpoint HTTP
 *
 * @author Taller IoT
 * @date 2026
 */

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <DNSServer.h>
#include "html_pages.h"

// ─── Configuración general ──────────────────────────────────────────────────
#define AP_SSID          "ESP32-Config"
#define AP_PASSWORD      ""           // Portal abierto (sin contraseña)
#define AP_IP            IPAddress(192, 168, 4, 1)
#define AP_GATEWAY       IPAddress(192, 168, 4, 1)
#define AP_SUBNET        IPAddress(255, 255, 255, 0)

#define DNS_PORT         53
#define HTTP_PORT        80

#define RESET_BUTTON_PIN 0            // GPIO0 = BOOT button en la mayoría de dev boards
#define RESET_HOLD_MS    3000         // 3 segundos para reset
#define CONNECT_TIMEOUT  15000        // 15 segundos para conectar
#define LED_PIN          2            // LED integrado

#define NVS_NAMESPACE    "wifi_cfg"
#define NVS_KEY_SSID     "ssid"
#define NVS_KEY_PASS     "password"
#define NVS_KEY_SAVED    "saved"

// ─── Estado del sistema ─────────────────────────────────────────────────────
enum SystemState {
  STATE_AP_MODE,        // Modo punto de acceso (sin credenciales)
  STATE_CONNECTING,     // Intentando conectar a WiFi
  STATE_CONNECTED,      // Conectado exitosamente
  STATE_RESET           // Reset solicitado
};

SystemState currentState = STATE_AP_MODE;

// ─── Objetos globales ───────────────────────────────────────────────────────
WebServer   server(HTTP_PORT);
Preferences prefs;
DNSServer   dnsServer;

String savedSSID     = "";
String savedPassword = "";
bool   isConfigured  = false;

unsigned long connectStartTime  = 0;
unsigned long resetButtonStart  = 0;
bool          resetButtonActive = false;

// ════════════════════════════════════════════════════════════════════════════
//  SETUP
// ════════════════════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n\n=== ESP32 WiFi Provisioning ===");

  pinMode(LED_PIN, OUTPUT);
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);

  loadCredentials();

  if (isConfigured) {
    Serial.printf("[INFO] Credenciales encontradas. SSID: %s\n", savedSSID.c_str());
    startConnecting();
  } else {
    Serial.println("[INFO] Sin credenciales. Iniciando modo AP...");
    startAPMode();
  }
}

// ════════════════════════════════════════════════════════════════════════════
//  LOOP
// ════════════════════════════════════════════════════════════════════════════
void loop() {
  handleResetButton();

  switch (currentState) {
    case STATE_AP_MODE:
      dnsServer.processNextRequest();
      server.handleClient();
      blinkLED(500);
      break;

    case STATE_CONNECTING:
      server.handleClient();
      checkConnection();
      blinkLED(200);
      break;

    case STATE_CONNECTED:
      server.handleClient();
      digitalWrite(LED_PIN, HIGH);
      break;

    case STATE_RESET:
      performReset();
      break;
  }
}

// ════════════════════════════════════════════════════════════════════════════
//  NVS – Carga / Guardado de credenciales
// ════════════════════════════════════════════════════════════════════════════

/**
 * @brief Lee credenciales almacenadas en NVS (Non-Volatile Storage).
 */
void loadCredentials() {
  prefs.begin(NVS_NAMESPACE, true); // solo lectura
  isConfigured  = prefs.getBool(NVS_KEY_SAVED, false);
  savedSSID     = prefs.getString(NVS_KEY_SSID, "");
  savedPassword = prefs.getString(NVS_KEY_PASS, "");
  prefs.end();

  Serial.printf("[NVS] Credenciales guardadas: %s\n", isConfigured ? "SÍ" : "NO");
}

/**
 * @brief Persiste las credenciales WiFi en NVS.
 * @param ssid     Nombre de la red
 * @param password Contraseña de la red
 */
void saveCredentials(const String& ssid, const String& password) {
  prefs.begin(NVS_NAMESPACE, false); // lectura/escritura
  prefs.putString(NVS_KEY_SSID, ssid);
  prefs.putString(NVS_KEY_PASS, password);
  prefs.putBool(NVS_KEY_SAVED, true);
  prefs.end();

  savedSSID     = ssid;
  savedPassword = password;
  isConfigured  = true;
  Serial.println("[NVS] Credenciales guardadas correctamente.");
}

/**
 * @brief Borra las credenciales WiFi del NVS.
 */
void clearCredentials() {
  prefs.begin(NVS_NAMESPACE, false);
  prefs.clear();
  prefs.end();

  savedSSID     = "";
  savedPassword = "";
  isConfigured  = false;
  Serial.println("[NVS] Credenciales borradas.");
}

// ════════════════════════════════════════════════════════════════════════════
//  MODO AP (Punto de Acceso + Portal Cautivo)
// ════════════════════════════════════════════════════════════════════════════

/**
 * @brief Inicia el ESP32 en modo Access Point con portal cautivo DNS.
 */
void startAPMode() {
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);
  WiFi.softAP(AP_SSID, AP_PASSWORD);

  // Portal cautivo: redirige todo el DNS al IP del AP
  dnsServer.start(DNS_PORT, "*", AP_IP);

  registerAPRoutes();
  server.begin();

  currentState = STATE_AP_MODE;
  Serial.printf("[AP] SSID: %s  IP: %s\n", AP_SSID, AP_IP.toString().c_str());
}

// ════════════════════════════════════════════════════════════════════════════
//  CONEXIÓN A RED CONFIGURADA
// ════════════════════════════════════════════════════════════════════════════

/**
 * @brief Inicia el proceso de conexión a la red WiFi guardada.
 */
void startConnecting() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(savedSSID.c_str(), savedPassword.c_str());

  connectStartTime = millis();
  currentState = STATE_CONNECTING;

  // Servidor disponible durante la conexión para feedback
  registerStationRoutes();
  server.begin();

  Serial.printf("[WiFi] Conectando a '%s'...\n", savedSSID.c_str());
}

/**
 * @brief Verifica el estado de la conexión. Si falla, vuelve al modo AP.
 */
void checkConnection() {
  if (WiFi.status() == WL_CONNECTED) {
    currentState = STATE_CONNECTED;
    Serial.printf("[WiFi] Conectado! IP: %s\n", WiFi.localIP().toString().c_str());
    return;
  }

  if (millis() - connectStartTime > CONNECT_TIMEOUT) {
    Serial.println("[WiFi] Timeout de conexión. Volviendo a modo AP...");
    WiFi.disconnect(true);
    delay(500);
    startAPMode();
  }
}

// ════════════════════════════════════════════════════════════════════════════
//  RUTAS HTTP – MODO AP
// ════════════════════════════════════════════════════════════════════════════
void registerAPRoutes() {
  // Portal cautivo: captura cualquier URL no reconocida
  server.onNotFound(handleCaptivePortal);

  // GET /          → Página principal del portal
  server.on("/", HTTP_GET, handleRoot);

  // GET /scan      → Lista redes disponibles (JSON)
  server.on("/scan", HTTP_GET, handleScan);

  // POST /connect  → Guarda credenciales e inicia conexión
  server.on("/connect", HTTP_POST, handleConnect);

  // GET /status    → Estado actual del dispositivo (JSON)
  server.on("/status", HTTP_GET, handleStatus);

  // POST /reset    → Borra credenciales y reinicia
  server.on("/reset", HTTP_POST, handleReset);
}

// ════════════════════════════════════════════════════════════════════════════
//  RUTAS HTTP – MODO STATION (conectado)
// ════════════════════════════════════════════════════════════════════════════
void registerStationRoutes() {
  server.on("/", HTTP_GET, handleConnectedRoot);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/reset", HTTP_POST, handleReset);
  server.onNotFound(handleNotFound);
}

// ════════════════════════════════════════════════════════════════════════════
//  HANDLERS HTTP
// ════════════════════════════════════════════════════════════════════════════

/**
 * @brief GET /
 * Devuelve el formulario HTML del portal de configuración.
 */
void handleRoot() {
  server.send(200, "text/html", getPortalHTML());
}

/**
 * @brief Portal cautivo: redirige cualquier URL al portal de configuración.
 */
void handleCaptivePortal() {
  server.sendHeader("Location", "http://" + AP_IP.toString(), true);
  server.send(302, "text/plain", "");
}

/**
 * @brief GET /scan
 * Escanea redes WiFi disponibles y responde con JSON.
 *
 * Respuesta 200:
 * {
 *   "networks": [
 *     { "ssid": "MiRed", "rssi": -65, "security": true },
 *     ...
 *   ]
 * }
 */
void handleScan() {
  server.sendHeader("Access-Control-Allow-Origin", "*");

  int n = WiFi.scanNetworks();
  String json = "{\"networks\":[";

  for (int i = 0; i < n; i++) {
    if (i > 0) json += ",";
    json += "{";
    json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
    json += "\"security\":" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "true" : "false");
    json += "}";
  }

  json += "]}";
  WiFi.scanDelete();
  server.send(200, "application/json", json);
}

/**
 * @brief POST /connect
 * Recibe credenciales WiFi, las guarda y reinicia el dispositivo.
 *
 * Body (application/x-www-form-urlencoded):
 *   ssid=MiRed&password=MiClave
 *
 * Respuestas:
 *   200 { "status": "connecting", "ssid": "MiRed" }
 *   400 { "error": "SSID requerido" }
 */
void handleConnect() {
  if (!server.hasArg("ssid") || server.arg("ssid").isEmpty()) {
    server.send(400, "application/json", "{\"error\":\"SSID requerido\"}");
    return;
  }

  String ssid     = server.arg("ssid");
  String password = server.arg("password");

  saveCredentials(ssid, password);

  String response = "{\"status\":\"connecting\",\"ssid\":\"" + ssid + "\"}";
  server.send(200, "application/json", response);

  Serial.println("[HTTP] Credenciales recibidas. Reiniciando en 2s...");
  delay(2000);
  ESP.restart();
}

/**
 * @brief GET /status
 * Devuelve el estado actual del sistema.
 *
 * Respuesta 200:
 * {
 *   "mode": "AP" | "STA",
 *   "connected": true | false,
 *   "ip": "192.168.x.x",
 *   "ssid": "NombreRed",
 *   "rssi": -70,
 *   "configured": true | false
 * }
 */
void handleStatus() {
  server.sendHeader("Access-Control-Allow-Origin", "*");

  bool connected = (WiFi.status() == WL_CONNECTED);
  String mode    = connected ? "STA" : "AP";
  String ip      = connected ? WiFi.localIP().toString() : AP_IP.toString();
  int rssi       = connected ? WiFi.RSSI() : 0;

  String json = "{";
  json += "\"mode\":\"" + mode + "\",";
  json += "\"connected\":" + String(connected ? "true" : "false") + ",";
  json += "\"ip\":\"" + ip + "\",";
  json += "\"ssid\":\"" + savedSSID + "\",";
  json += "\"rssi\":" + String(rssi) + ",";
  json += "\"configured\":" + String(isConfigured ? "true" : "false");
  json += "}";

  server.send(200, "application/json", json);
}

/**
 * @brief POST /reset
 * Borra las credenciales guardadas y reinicia el dispositivo en modo AP.
 *
 * Respuesta 200:
 * { "status": "reset", "message": "Configuración borrada. Reiniciando..." }
 */
void handleReset() {
  clearCredentials();

  server.send(200, "application/json",
    "{\"status\":\"reset\",\"message\":\"Configuracion borrada. Reiniciando...\"}");

  Serial.println("[HTTP] Reset solicitado. Reiniciando...");
  delay(1500);
  ESP.restart();
}

/**
 * @brief GET / (modo conectado)
 * Página de información cuando el dispositivo ya está conectado.
 */
void handleConnectedRoot() {
  server.send(200, "text/html", getConnectedHTML(
    savedSSID,
    WiFi.localIP().toString(),
    String(WiFi.RSSI())
  ));
}

/**
 * @brief 404 para rutas no encontradas en modo STA.
 */
void handleNotFound() {
  server.send(404, "application/json", "{\"error\":\"Ruta no encontrada\"}");
}

// ════════════════════════════════════════════════════════════════════════════
//  RESET POR BOTÓN FÍSICO
// ════════════════════════════════════════════════════════════════════════════

/**
 * @brief Detecta pulsación larga del botón BOOT para resetear credenciales.
 *        Mantener pulsado RESET_HOLD_MS milisegundos para activar.
 */
void handleResetButton() {
  if (digitalRead(RESET_BUTTON_PIN) == LOW) {
    if (!resetButtonActive) {
      resetButtonActive = true;
      resetButtonStart  = millis();
      Serial.println("[BTN] Botón presionado...");
    } else if (millis() - resetButtonStart >= RESET_HOLD_MS) {
      Serial.println("[BTN] Reset por botón activado!");
      currentState = STATE_RESET;
    }
  } else {
    resetButtonActive = false;
  }
}

/**
 * @brief Ejecuta el reset: borra credenciales y reinicia.
 */
void performReset() {
  clearCredentials();
  Serial.println("[RESET] Reiniciando en modo AP...");
  delay(500);
  ESP.restart();
}

// ════════════════════════════════════════════════════════════════════════════
//  UTILIDADES
// ════════════════════════════════════════════════════════════════════════════

/**
 * @brief Parpadea el LED integrado con el período indicado.
 * @param period_ms Período de parpadeo en milisegundos
 */
void blinkLED(unsigned long period_ms) {
  static unsigned long last = 0;
  static bool state = false;
  if (millis() - last >= period_ms / 2) {
    last = millis();
    state = !state;
    digitalWrite(LED_PIN, state);
  }
}
