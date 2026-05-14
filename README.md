# ESP32 WiFi Provisioning — Portal Cautivo

> Aprovisionamiento dinámico de credenciales WiFi para ESP32 mediante interfaz web local (portal cautivo). Sin necesidad de reprogramar el dispositivo.

---

## Tabla de contenidos

1. [Descripción del sistema](#descripción-del-sistema)
2. [Diagrama de flujo de estados](#diagrama-de-flujo-de-estados)
3. [Diagrama UML de secuencia](#diagrama-uml-de-secuencia)
4. [Documentación de endpoints](#documentación-de-endpoints)
5. [Instalación y dependencias](#instalación-y-dependencias)
6. [Estructura del proyecto](#estructura-del-proyecto)
7. [Mecanismo de reset](#mecanismo-de-reset)
8. [Preguntas del taller](#preguntas-del-taller)

---

## Descripción del sistema

El ESP32 arranca en **modo Access Point** si no tiene credenciales WiFi guardadas. El usuario se conecta a la red `ESP32-Config` y es redirigido automáticamente (portal cautivo DNS) a `http://192.168.4.1`, donde puede:

- Escanear redes disponibles
- Ingresar SSID y contraseña
- Guardar las credenciales (NVS – Non-Volatile Storage)
- Reiniciar el dispositivo para que se conecte a la red configurada

Una vez conectado, el dispositivo expone una página de estado y permite resetear la configuración para cambiar de red.

---

## Diagrama de flujo de estados

```
         ┌───────────────┐
         │     INICIO    │
         └──────┬────────┘
                │
        ┿ ¿Hay credenciales en NVS?
       /                 \
     NO                  SÍ
      │                   │
      ▼                   ▼
┌──────────┐       ┌─────────────┐
│ MODO AP  │       │ CONNECTING  │
│(portal   │       │(intento de  │
│ cautivo) │       │ conexión)   │
└────┬─────┘       └──────┬──────┘
     │                    │
     │  POST /connect  ┿ ¿Conectado en < 15 s?
     │   (guardar)    /           \
     │              SÍ             NO
     │               │              │
     │               ▼              ▼
     │        ┌──────────┐    ┌──────────┐
     │        │ CONNECTED│    │ MODO AP  │ ← timeout
     │        │(STA mode)│    │(de nuevo)│
     │        └────┬─────┘    └──────────┘
     │             │
     │    POST /reset  ó  botón BOOT 3s
     │             │
     └─────────────▼
              ┌──────────┐
              │  RESET   │
              │(borra NVS│
              │ restart) │
              └──────────┘
```

---

## Diagrama UML de secuencia

```
Usuario            ESP32 (AP)             NVS             Red WiFi
   │                   │                   │                  │
   │  Conecta a        │                   │                  │
   │  "ESP32-Config"   │                   │                  │
   │──────────────────►│                   │                  │
   │                   │                   │                  │
   │  GET /            │                   │                  │
   │──────────────────►│                   │                  │
   │◄── 200 HTML ──────│                   │                  │
   │                   │                   │                  │
   │  GET /scan        │                   │                  │
   │──────────────────►│                   │                  │
   │                   │── WiFi.scan() ───►│                  │
   │◄── 200 JSON ──────│                   │                  │
   │  (lista redes)    │                   │                  │
   │                   │                   │                  │
   │  POST /connect    │                   │                  │
   │  {ssid, password} │                   │                  │
   │──────────────────►│                   │                  │
   │                   │── prefs.put() ───►│                  │
   │                   │                   │                  │
   │◄── 200 JSON ──────│                   │                  │
   │  {connecting}     │                   │                  │
   │                   │                   │                  │
   │                   │── ESP.restart()   │                  │
   │                   │                   │                  │
   │          [ESP32 reinicia en modo STA] │                  │
   │                   │                   │                  │
   │                   │── prefs.get() ───►│                  │
   │                   │◄── ssid, pass ────│                  │
   │                   │                   │                  │
   │                   │── WiFi.begin() ──────────────────────►│
   │                   │◄─ WL_CONNECTED ───────────────────────│
   │                   │                   │                  │
   │  GET /status      │                   │                  │
   │──────────────────►│                   │                  │
   │◄── 200 JSON ──────│                   │                  │
   │  {mode:STA, ...}  │                   │                  │
   │                   │                   │                  │
   │  POST /reset      │                   │                  │
   │──────────────────►│                   │                  │
   │                   │── prefs.clear() ─►│                  │
   │◄── 200 JSON ──────│                   │                  │
   │  {reset}          │── ESP.restart()   │                  │
   │                   │                   │                  │
```

---

## Documentación de endpoints

> Base URL modo AP: `http://192.168.4.1`  
> Base URL modo STA: `http://<IP_asignada>`

---

### `GET /`

Devuelve la página HTML del portal de configuración (modo AP) o la página de estado (modo STA conectado).

| Campo    | Valor                |
|----------|----------------------|
| URL      | `/`                  |
| Método   | `GET`                |
| Headers  | —                    |
| Query    | —                    |
| Payload  | —                    |

**Respuesta 200 OK**
```
Content-Type: text/html
<Página HTML del portal>
```

---

### `GET /scan`

Escanea redes WiFi disponibles y retorna la lista en JSON.

| Campo    | Valor                        |
|----------|------------------------------|
| URL      | `/scan`                      |
| Método   | `GET`                        |
| Headers  | `Accept: application/json`   |
| Query    | —                            |
| Payload  | —                            |

**Respuesta 200 OK**
```json
{
  "networks": [
    { "ssid": "MiRed",      "rssi": -52, "security": true  },
    { "ssid": "RedAbierta", "rssi": -78, "security": false }
  ]
}
```

| Campo      | Tipo    | Descripción                              |
|------------|---------|------------------------------------------|
| ssid       | string  | Nombre de la red WiFi                    |
| rssi       | integer | Intensidad de señal en dBm               |
| security   | boolean | `true` si la red requiere contraseña     |

---

### `POST /connect`

Recibe las credenciales WiFi, las persiste en NVS y reinicia el dispositivo.

| Campo    | Valor                                       |
|----------|---------------------------------------------|
| URL      | `/connect`                                  |
| Método   | `POST`                                      |
| Headers  | `Content-Type: application/x-www-form-urlencoded` |
| Query    | —                                           |

**Payload (form-urlencoded)**

| Parámetro | Requerido | Tipo   | Descripción          |
|-----------|-----------|--------|----------------------|
| ssid      | ✅ Sí     | string | Nombre de la red     |
| password  | ❌ No     | string | Contraseña (vacío si es red abierta) |

**Respuesta 200 OK**
```json
{
  "status": "connecting",
  "ssid": "MiRed"
}
```

**Respuesta 400 Bad Request**
```json
{
  "error": "SSID requerido"
}
```

---

### `GET /status`

Retorna el estado actual del sistema.

| Campo    | Valor                        |
|----------|------------------------------|
| URL      | `/status`                    |
| Método   | `GET`                        |
| Headers  | `Accept: application/json`   |
| Query    | —                            |
| Payload  | —                            |

**Respuesta 200 OK**
```json
{
  "mode":       "STA",
  "connected":  true,
  "ip":         "192.168.1.45",
  "ssid":       "MiRed",
  "rssi":       -61,
  "configured": true
}
```

| Campo       | Tipo    | Valores posibles          |
|-------------|---------|---------------------------|
| mode        | string  | `"AP"` · `"STA"`          |
| connected   | boolean | Estado de la conexión WiFi|
| ip          | string  | Dirección IP asignada     |
| ssid        | string  | SSID configurado          |
| rssi        | integer | Señal en dBm (0 si AP)    |
| configured  | boolean | Si hay credenciales en NVS|

---

### `POST /reset`

Borra las credenciales almacenadas en NVS y reinicia el dispositivo en modo AP.

| Campo    | Valor           |
|----------|-----------------|
| URL      | `/reset`        |
| Método   | `POST`          |
| Headers  | —               |
| Query    | —               |
| Payload  | —               |

**Respuesta 200 OK**
```json
{
  "status":  "reset",
  "message": "Configuracion borrada. Reiniciando..."
}
```

---

## Instalación y dependencias

### Hardware
- ESP32 DevKit (cualquier variante)
- Cable USB-A a Micro-USB / USB-C

### Software
- **Arduino IDE** ≥ 2.x  
- **Board**: `esp32` by Espressif Systems ≥ 2.0.0  
  - Menú → Preferencias → URL adicional: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`

### Librerías utilizadas (incluidas en el SDK de esp32)

| Librería     | Propósito                              |
|--------------|----------------------------------------|
| `WiFi.h`     | Conexión WiFi (AP + STA)               |
| `WebServer.h`| Servidor HTTP en puerto 80             |
| `Preferences.h` | Almacenamiento NVS no volátil       |
| `DNSServer.h`| Portal cautivo (redirige todo el DNS)  |

> **No se requieren librerías externas.** Todo viene con el paquete de Espressif.

### Configuración de placa en Arduino IDE

| Parámetro               | Valor recomendado  |
|-------------------------|--------------------|
| Board                   | ESP32 Dev Module   |
| Upload Speed            | 921600             |
| Flash Size              | 4MB (32Mb)         |
| Partition Scheme        | Default 4MB with spiffs |
| Core Debug Level        | None               |

---

## Estructura del proyecto

```
esp32_wifi_provisioning/
├── esp32_provisioning.ino   # Sketch principal
├── html_pages.h             # Páginas HTML embebidas
└── README.md                # Este archivo
```

---

## Mecanismo de reset

El sistema provee **dos mecanismos** para restablecer la configuración:

### 1. Botón físico (GPIO0 / BOOT)
Mantén pulsado el botón **BOOT** durante **3 segundos** mientras el dispositivo está en funcionamiento. El LED parpadeará rápidamente antes del reinicio.

### 2. Endpoint HTTP `POST /reset`
Desde cualquier navegador o cliente HTTP:

```bash
# Desde modo STA (conectado)
curl -X POST http://<IP_ESP32>/reset

# Desde modo AP
curl -X POST http://192.168.4.1/reset
```

O usando el botón en la interfaz web.

---

## Preguntas del taller

### 1. ¿Es posible conectarse a redes WiFi con seguridad PEAP Enterprise con el ESP32?

**Sí, es posible.** El ESP32 soporta WPA2-Enterprise (EAP) a través del componente `esp_wpa2` del framework ESP-IDF, accesible también desde Arduino.

**Lo que se necesita:**

```cpp
#include "esp_wpa2.h"

WiFi.mode(WIFI_STA);
esp_wifi_sta_wpa2_ent_set_identity((uint8_t*)EAP_ID, strlen(EAP_ID));
esp_wifi_sta_wpa2_ent_set_username((uint8_t*)EAP_USERNAME, strlen(EAP_USERNAME));
esp_wifi_sta_wpa2_ent_set_password((uint8_t*)EAP_PASSWORD, strlen(EAP_PASSWORD));
esp_wifi_sta_wpa2_ent_enable();
WiFi.begin(SSID);
```

Datos adicionales requeridos para PEAP:
- **EAP Identity** (identidad anónima, ej. `anonymous@dominio.com`)
- **Username** (usuario interno)
- **Password**
- Opcionalmente: **certificado CA** del servidor RADIUS (para validación del servidor)

**Limitaciones conocidas:**
- No soporta todas las variantes de EAP (solo PEAP, TTLS y TLS)
- Los certificados deben estar en formato DER o PEM embebido en el firmware
- Requiere más memoria que WPA2-Personal

---

### 2. ¿Cuántas conexiones simultáneas soporta la librería WebServer?

La librería `WebServer` de Arduino-ESP32 soporta **por defecto 1 cliente a la vez** de forma secuencial. Internamente usa `WiFiServer` con `setNoDelay(true)` pero procesa las solicitudes de una en una en el loop principal.

**Alternativas:**

| Alternativa        | Conexiones | Descripción                                              |
|--------------------|------------|----------------------------------------------------------|
| `WebServer`        | ~1         | Simple, blocking, adecuada para portales de config       |
| `ESPAsyncWebServer`| ~4-8       | Asíncrona, no bloquea el loop, soporta WebSocket         |
| `ESP-IDF HTTP Server` | Configurable | Nativa, más robusta, múltiples hilos              |
| `mongoose`         | Múltiple   | Framework C embebido, alto rendimiento                   |

Para un **portal de aprovisionamiento** (uso ocasional, 1 usuario a la vez), `WebServer` es completamente suficiente.

---

### 3. Comparación de memoria Flash usada

> Medición con ESP32 Dev Module, Arduino IDE 2.x, Espressif ESP32 SDK 2.0.x
> Partition scheme: Default 4MB with spiffs

| Implementación                        | Flash usada | Porcentaje |
|---------------------------------------|-------------|------------|
| **Esta implementación**               | ~410 KB     | ~31%       |
| **WiFiManager – Ejemplo "Basic"**     | ~720 KB     | ~55%       |

**Diferencia aproximada: ~310 KB menos** usando implementación propia.

**Razones de la diferencia:**
- WiFiManager incluye soporte para múltiples modos (mDNS, OTA update, parámetros personalizados)
- WiFiManager usa `SPIFFS` para los archivos HTML, lo cual tiene overhead
- Esta implementación usa `Preferences` (NVS) más liviano que SPIFFS
- Esta implementación embebe el HTML como `String` en memoria de programa, sin sistema de archivos

> ⚠️ Los valores exactos varían según versión del SDK y opciones de compilación. Se recomienda medir en el entorno propio con la opción *Mostrar verbose al compilar* activada en Arduino IDE.
