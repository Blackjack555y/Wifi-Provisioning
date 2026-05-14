# Preguntas sobre ESP32 — Redes y Servidores Web

**Materia:** IoT  
**Fecha:** 14 de Mayo 2026

---

## 1. ¿Es posible conectarse a redes WiFi con seguridad PEAP Enterprise con el ESP32? ¿Qué se necesita?

### Respuesta

Sí, el ESP32 **sí soporta conexiones a redes WiFi Enterprise con autenticación PEAP**. Esto lo diferencia de su antecesor, el ESP8266, que no tiene soporte para este tipo de redes.

PEAP (Protected Extensible Authentication Protocol) es el protocolo usado en redes institucionales o corporativas, como las redes *eduroam* de universidades. A diferencia de una red WiFi casera donde solo se necesita una contraseña, aquí el proceso de autenticación es más complejo: hay un servidor RADIUS que verifica la identidad del cliente antes de darle acceso.

### ¿Qué se necesita para conectarse?

**Datos necesarios:**

| Parámetro | Descripción |
|---|---|
| `SSID` | Nombre de la red |
| `EAP Identity` | Identidad externa (puede ser usuario o MAC del dispositivo) |
| `EAP Username` | Nombre de usuario para la autenticación interna |
| `EAP Password` | Contraseña del usuario |
| `CA Certificate` | Certificado del servidor RADIUS (opcional, pero recomendado) |

**Librerías necesarias:**

```cpp
#include <WiFi.h>
#include "esp_wpa2.h"  // Librería de Espressif para WPA2 Enterprise
```

**Ejemplo básico de conexión:**

```cpp
#include <WiFi.h>
#include "esp_wpa2.h"

const char* ssid       = "eduroam";
const char* identity   = "usuario@universidad.edu";
const char* username   = "usuario";
const char* password   = "mi_contraseña";

void setup() {
    Serial.begin(115200);
    WiFi.disconnect(true);

    // Configurar credenciales WPA2 Enterprise
    esp_wifi_sta_wpa2_ent_set_identity((uint8_t*)identity, strlen(identity));
    esp_wifi_sta_wpa2_ent_set_username((uint8_t*)username, strlen(username));
    esp_wifi_sta_wpa2_ent_set_password((uint8_t*)password, strlen(password));
    esp_wifi_sta_wpa2_ent_enable();

    WiFi.begin(ssid);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConectado!");
    Serial.println(WiFi.localIP());
}
```

También existe una librería de más alto nivel llamada **ESP32WiFiEnterprise** (disponible en el Library Manager de Arduino) que simplifica aún más este proceso.

### Consideraciones importantes

- La conexión PEAP tarda **hasta 6 segundos**, más que una red WPA2-PSK normal, porque implica un handshake TLS adicional.
- Si el certificado CA del servidor RADIUS es muy grande (por ejemplo, 20 KB), puede haber problemas de memoria (heap insuficiente) durante el handshake.
- Algunas redes Enterprise usan únicamente **TLS 1.0** (ya deprecado). Los ESP32 modernos negocian la versión más alta disponible, lo que puede causar fallas de conexión con servidores antiguos.

---

## 2. ¿Cuántas conexiones/clientes simultáneos soporta la librería WebServer? ¿Qué alternativas hay?

### Respuesta

La librería **`WebServer`** que viene incluida en el core de Arduino para ESP32 es un servidor **síncrono (bloqueante)**. Esto significa que atiende **una sola petición a la vez**: mientras está respondiendo a un cliente, los demás deben esperar.

En la práctica, funciona así:

```
Cliente A ──────────────────────────────────► (atendido)
Cliente B ───────────────────────────────────────────────────────► (espera)
Cliente C ────────────────────────────────────────────────────────────────► (espera aún más)
```

Aunque el navegador puede abrir varias conexiones TCP en paralelo (HTTP keep-alive), el servidor las procesa de forma **secuencial** en el `loop()`, lo que genera lentitud perceptible con múltiples clientes.

> **Limitación práctica:** Abrir una página web en un solo navegador puede llegar a abrir hasta 4 sockets simultáneamente (para cargar HTML, CSS, JS, imágenes). Si el ESP32 tiene un límite de 16 sockets en lwIP y los navegadores no los cierran correctamente al navegar, estos se agotan rápidamente.

### Alternativas

#### ESPAsyncWebServer *(la más popular)*

Utiliza un modelo **asíncrono (no bloqueante)**. Puede manejar múltiples clientes simultáneamente porque, mientras envía la respuesta a un cliente, ya está listo para atender al siguiente.

```cpp
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

AsyncWebServer server(80);

void setup() {
    WiFi.begin(ssid, password);
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(200, "text/plain", "Hola!");
    });
    server.begin();
}

void loop() {
    // No necesita handleClient() — funciona en segundo plano
}
```

#### Comparación de alternativas

| Librería | Modelo | Clientes simultáneos | Complejidad | Uso de RAM |
|---|---|---|---|---|
| `WebServer` (built-in) | Síncrono | 1 (secuencial) | Baja | Baja |
| `ESPAsyncWebServer` | Asíncrono | Varios | Media | Media-Alta |
| `PsychicHttp` | Asíncrono (ESP-IDF) | Varios | Media | Media |
| `ESP-IDF httpd` | Configurable | Configurable (`max_open_sockets`) | Alta | Configurable |

**Recomendación:** Para proyectos IoT donde varios usuarios acceden al panel de control del ESP32 al mismo tiempo, se recomienda usar **ESPAsyncWebServer** (actualmente mantenida por la organización ESP32Async, versión 3.9.4).

---

## 3. Comparación de memoria Flash: implementación propia vs. ejemplo "Basic" de WiFiManager

### Respuesta

Esta comparación depende de la implementación específica, pero se puede analizar qué incluye cada opción y cómo eso impacta el uso de Flash.

### ¿Qué incluye cada opción?

**Ejemplo "Basic" de WiFiManager (tzapu):**

WiFiManager es una librería que automatiza la configuración de WiFi mediante un portal cautivo. El ejemplo básico incluye internamente:
- Un servidor web embebido (para el portal de configuración)
- Lógica de escaneo de redes
- HTML, CSS y JS embebidos para la página de configuración
- Manejo de credenciales en Flash (NVS/EEPROM)

Esto hace que el sketch sea considerablemente más pesado, ocupando típicamente entre **700 y 900 KB** de Flash en un ESP32.

**Implementación propia con `WebServer`:**

Un sketch que solo usa `WiFi.h` + `WebServer.h` para conectarse y servir páginas es mucho más liviano. Por referencia, un sketch básico que solo conecta a WiFi ya usa cerca de **625 KB** de Flash. Agregar `WebServer.h` suma unos pocos KB más, resultando en un total de aproximadamente **650–700 KB**.

### ¿Cómo medir el uso real?

Para hacer la comparación exacta, se puede compilar ambos sketches en Arduino IDE y observar la salida en la consola:

```
Sketch uses XXXXX bytes (XX%) of program storage space. Maximum is 1310720 bytes.
```

### Tabla comparativa estimada

| Implementación | Flash usada (aprox.) | ¿Incluye portal de configuración? |
|---|---|---|
| WiFiManager — ejemplo "Basic" | ~800–900 KB | Sí (automático) |
| WebServer propio (solo WiFi + WebServer) | ~650–700 KB | No (manual) |
| **Diferencia aproximada** | **~150–250 KB** | — |

> **Nota:** Si la implementación propia usa **ESPAsyncWebServer** en lugar de `WebServer`, el tamaño puede ser **mayor** que el ejemplo Basic de WiFiManager, ya que AsyncTCP + ESPAsyncWebServer añaden más código que la librería `WebServer` estándar. La ventaja en tamaño de Flash aplica principalmente cuando se usa `WebServer` built-in.

### Conclusión

Si el objetivo es minimizar el uso de Flash y no se necesita un portal de configuración automático, una implementación propia con `WebServer` es la opción más liviana. Si se necesita comodidad para cambiar credenciales sin recompilar, WiFiManager justifica su overhead adicional en Flash.

---

*Fuentes: Documentación oficial de Espressif (arduino-esp32), foros ESP32, Random Nerd Tutorials, GitHub espressif/arduino-esp32.*
