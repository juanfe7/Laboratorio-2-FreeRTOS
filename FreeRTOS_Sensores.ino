/*
  FreeRTOS_Sensores.ino
  =====================
  Sistema con:
    - 2 tareas lectoras de sensores capacitivos (misma función, distintos params)
    - 2 colas independientes con timestamps
    - 2 tareas escritoras serie (misma función, distintos params)
    - 1 Mutex para evitar acceso concurrente al puerto serial

  Plataforma: ESP32 (FreeRTOS nativo) o Arduino Due/Teensy con FreeRTOS
  Para ESP32 los pines táctiles son: T0=GPIO4, T1=GPIO0, T2=GPIO2,
  T3=GPIO15, T4=GPIO13, T5=GPIO12, T6=GPIO14, T7=GPIO27, T8=GPIO33, T9=GPIO32
*/

#include <Arduino.h>

// ─────────────────────────────────────────────
// CONFIGURACIÓN DE PINES (ajustar según board)
// ─────────────────────────────────────────────
#define TOUCH_PIN_1   T0   // GPIO 4 en ESP32
#define TOUCH_PIN_2   T3   // GPIO 15 en ESP32

// Umbral táctil (valor < umbral = toque detectado)
#define TOUCH_THRESHOLD  40

// Prioridades de tareas (mayor número = mayor prioridad)
#define PRIORITY_SENSOR  2
#define PRIORITY_SERIAL  1

// Tamaño de las colas (número de elementos)
#define QUEUE_SIZE  10

// Delay entre lecturas (ms)
#define SENSOR_READ_DELAY_MS  200

// ─────────────────────────────────────────────
// TIPOS DE DATOS
// ─────────────────────────────────────────────

/**
 * Estructura de dato que se envía por la cola.
 * Incluye el valor leído y el timestamp en milisegundos.
 */
typedef struct {
  uint32_t value;      // Valor raw del sensor táctil
  uint32_t timestamp;  // millis() al momento de lectura
  bool     touched;    // true si se detectó toque
} SensorData_t;

/**
 * Parámetros que recibe la tarea lectora de sensor.
 */
typedef struct {
  uint8_t         pin;        // Pin táctil del ESP32
  QueueHandle_t   queue;      // Puntero a la cola destino
  uint8_t         sensorId;   // ID del sensor (1 o 2)
} SensorTaskParams_t;

/**
 * Parámetros que recibe la tarea escritora serie.
 */
typedef struct {
  QueueHandle_t   queue;      // Puntero a la cola origen
  SemaphoreHandle_t mutex;    // Mutex del puerto serial
  uint8_t         sensorId;   // ID del sensor (1 o 2)
} SerialTaskParams_t;

// ─────────────────────────────────────────────
// HANDLES GLOBALES
// ─────────────────────────────────────────────
QueueHandle_t     queue1     = NULL;
QueueHandle_t     queue2     = NULL;
SemaphoreHandle_t serialMutex = NULL;

TaskHandle_t sensorTask1Handle = NULL;
TaskHandle_t sensorTask2Handle = NULL;
TaskHandle_t serialTask1Handle = NULL;
TaskHandle_t serialTask2Handle = NULL;

// Estructuras de parámetros (deben persistir durante toda la ejecución)
SensorTaskParams_t sensorParams1;
SensorTaskParams_t sensorParams2;
SerialTaskParams_t serialParams1;
SerialTaskParams_t serialParams2;

// ─────────────────────────────────────────────
// FUNCIÓN A: Tarea lectora de sensor (reutilizable)
// ─────────────────────────────────────────────
/**
 * Tarea genérica de lectura de sensor táctil.
 * 
 * Se crean dos instancias de esta función con distintos parámetros:
 *   - Instancia A: sensor 1, pin TOUCH_PIN_1, queue1
 *   - Instancia B: sensor 2, pin TOUCH_PIN_2, queue2
 * 
 * @param pvParameters  Puntero a SensorTaskParams_t casteado a void*
 */
void sensorReadTask(void* pvParameters) {
  // Convertir el parámetro genérico al tipo específico
  SensorTaskParams_t* params = (SensorTaskParams_t*) pvParameters;

  SensorData_t data;

  Serial.print("[Sensor ");
  Serial.print(params->sensorId);
  Serial.println("] Tarea iniciada.");

  for (;;) {  // Loop infinito de la tarea
    // Leer el valor capacitivo del pin
    data.value     = touchRead(params->pin);
    data.timestamp = millis();
    data.touched   = (data.value < TOUCH_THRESHOLD);

    // Intentar insertar en la cola (espera máx. 10ms)
    // Si la cola está llena, xQueueSend retorna errQUEUE_FULL
    BaseType_t result = xQueueSend(params->queue, &data, pdMS_TO_TICKS(10));

    if (result == errQUEUE_FULL) {
      // Cola llena: se descarta el dato (política configurable)
      // Alternativa: xQueueOverwrite() para sobreescribir el más antiguo
      Serial.print("[WARN] Cola del sensor ");
      Serial.print(params->sensorId);
      Serial.println(" llena. Dato descartado.");
    }

    // Esperar antes de la siguiente lectura
    vTaskDelay(pdMS_TO_TICKS(SENSOR_READ_DELAY_MS));
  }
}

// ─────────────────────────────────────────────
// FUNCIÓN B: Tarea escritora serial (reutilizable)
// ─────────────────────────────────────────────
/**
 * Tarea genérica de envío de datos por serial en formato JSON.
 * 
 * Se crean dos instancias de esta función con distintos parámetros:
 *   - Instancia A: lee de queue1, id=1
 *   - Instancia B: lee de queue2, id=2
 * 
 * Usa un Mutex para garantizar que solo una tarea escribe a la vez.
 * 
 * @param pvParameters  Puntero a SerialTaskParams_t casteado a void*
 */
void serialWriteTask(void* pvParameters) {
  SerialTaskParams_t* params = (SerialTaskParams_t*) pvParameters;

  SensorData_t data;
  char jsonBuffer[128];

  Serial.print("[Serial ");
  Serial.print(params->sensorId);
  Serial.println("] Tarea iniciada.");

  for (;;) {
    // Esperar indefinidamente hasta que haya datos en la cola
    if (xQueueReceive(params->queue, &data, portMAX_DELAY) == pdTRUE) {

      // Formatear JSON en el buffer
      snprintf(jsonBuffer, sizeof(jsonBuffer),
        "{\"sensor\":%u,\"value\":%lu,\"touched\":%s,\"timestamp\":%lu}",
        params->sensorId,
        (unsigned long) data.value,
        data.touched ? "true" : "false",
        (unsigned long) data.timestamp
      );

      // ── Sección crítica: adquirir mutex antes de escribir ──
      // xSemaphoreTake bloquea esta tarea si otra ya tiene el mutex
      if (xSemaphoreTake(params->mutex, pdMS_TO_TICKS(100)) == pdTRUE) {

        Serial.println(jsonBuffer);

        // Liberar el mutex para que la otra tarea pueda escribir
        xSemaphoreGive(params->mutex);
      } else {
        // Timeout al esperar el mutex (no debería ocurrir en condiciones normales)
        Serial.println("[ERROR] Timeout esperando mutex serial.");
      }
    }
  }
}

// ─────────────────────────────────────────────
// SETUP
// ─────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(1000);  // Esperar inicialización del serial
  Serial.println("\n[BOOT] Iniciando sistema FreeRTOS...");

  // ── Crear colas ──────────────────────────────
  queue1 = xQueueCreate(QUEUE_SIZE, sizeof(SensorData_t));
  queue2 = xQueueCreate(QUEUE_SIZE, sizeof(SensorData_t));

  if (queue1 == NULL || queue2 == NULL) {
    Serial.println("[ERROR] No se pudieron crear las colas.");
    while (true);  // Detener ejecución
  }
  Serial.println("[OK] Colas creadas.");

  // ── Crear mutex ───────────────────────────────
  serialMutex = xSemaphoreCreateMutex();
  if (serialMutex == NULL) {
    Serial.println("[ERROR] No se pudo crear el mutex.");
    while (true);
  }
  Serial.println("[OK] Mutex creado.");

  // ── Configurar parámetros ─────────────────────
  // Tarea lectora 1
  sensorParams1.pin      = TOUCH_PIN_1;
  sensorParams1.queue    = queue1;
  sensorParams1.sensorId = 1;

  // Tarea lectora 2
  sensorParams2.pin      = TOUCH_PIN_2;
  sensorParams2.queue    = queue2;
  sensorParams2.sensorId = 2;

  // Tarea escritora 1
  serialParams1.queue    = queue1;
  serialParams1.mutex    = serialMutex;
  serialParams1.sensorId = 1;

  // Tarea escritora 2
  serialParams2.queue    = queue2;
  serialParams2.mutex    = serialMutex;
  serialParams2.sensorId = 2;

  // ── Crear tareas ──────────────────────────────
  // Las dos tareas SENSOR ejecutan la MISMA función "sensorReadTask"
  // con DISTINTOS parámetros (pin y cola diferentes)
  xTaskCreate(
    sensorReadTask,        // Función A (reutilizable)
    "SensorTask1",         // Nombre para depuración
    2048,                  // Tamaño del stack en bytes
    &sensorParams1,        // Parámetro → void* casteado dentro de la función
    PRIORITY_SENSOR,       // Prioridad
    &sensorTask1Handle     // Handle (opcional)
  );

  xTaskCreate(
    sensorReadTask,        // Misma función A
    "SensorTask2",
    2048,
    &sensorParams2,        // Distintos parámetros
    PRIORITY_SENSOR,
    &sensorTask2Handle
  );

  // Las dos tareas SERIAL ejecutan la MISMA función "serialWriteTask"
  // con DISTINTOS parámetros (cola y id diferentes)
  xTaskCreate(
    serialWriteTask,       // Función B (reutilizable)
    "SerialTask1",
    2048,
    &serialParams1,
    PRIORITY_SERIAL,
    &serialTask1Handle
  );

  xTaskCreate(
    serialWriteTask,       // Misma función B
    "SerialTask2",
    2048,
    &serialParams2,        // Distintos parámetros
    PRIORITY_SERIAL,
    &serialTask2Handle
  );

  Serial.println("[OK] Tareas creadas. Scheduler activo.\n");
  // En ESP32 el scheduler ya corre; en Arduino Due se requiere vTaskStartScheduler()
}

// ─────────────────────────────────────────────
// LOOP (no usado — FreeRTOS toma el control)
// ─────────────────────────────────────────────
void loop() {
  // En ESP32 con FreeRTOS, loop() corre como la tarea "loopTask"
  // con prioridad 1. Podría usarse para monitoreo, pero en este
  // diseño todas las tareas son independientes.
  vTaskDelay(pdMS_TO_TICKS(5000));
}
