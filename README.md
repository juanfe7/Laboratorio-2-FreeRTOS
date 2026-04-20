# Práctica FreeRTOS y Arquitectura de Proyecto

Este apartado contiene el desarrollo de la práctica de sistemas de tiempo real y la propuesta arquitectónica para el proyecto final del curso.

---

## 1. Diagrama de la Práctica (FreeRTOS)
El siguiente diagrama detalla la implementación de tareas concurrentes, colas de mensajes y el control de acceso al puerto serial mediante un Mutex.

<div align="center">
  <img width="452" height="283" alt="image" src="https://github.com/user-attachments/assets/2926e599-2758-4350-b91d-0127645a07e4" />
  <p><i>Figura 1: Arquitectura de tareas, colas y sincronización por Mutex.</i></p>
</div>

### Explicación de la Práctica
- **Reutilización de Funciones:** Se implementaron dos tareas de lectura que ejecutan la misma lógica con diferentes parámetros (Pines de sensores touch).
- **Comunicación por Colas:** Los datos capturados (valor + timestamp) se envían en formato JSON a colas independientes.
- **Protección de Recurso Compartido:** Se utiliza un Mutex para garantizar que solo una tarea a la vez escriba en el puerto Serial, evitando la corrupción de los datos JSON.

---

## 2. Arquitectura Hipotética del Proyecto Final
Se plantea una arquitectura robusta para el Smart Car ESP32 utilizando las capacidades de doble núcleo del microcontrolador y el ecosistema de FreeRTOS.

<div align="center">
  <img width="556" height="294" alt="image" src="https://github.com/user-attachments/assets/cf2b53ed-d64d-41af-a4d1-19eb442a1a48" />
  <p><i>Figura 2: Propuesta de arquitectura multi-core para el Smart Car.</i></p>
</div>

### Componentes de la Arquitectura
- **Multitarea (Dual Core):** El servidor Web se asigna al Core 1 para manejar la conectividad Wi-Fi, mientras que la telemetría y el control de motores corren en el Core 0.
- **Gestión de Datos (Queues):** Se emplean colas para comunicar la interrupción del sensor de velocidad (ISR) con la tarea de cálculo de RPM.
- **Concurrencia (Mutex):** Un Mutex protege la estructura de "Estado Global" del vehículo, permitiendo lecturas y escrituras seguras entre la interfaz web y el control físico.

---

## 3. Respuestas a Preguntas Técnicas

### ¿Cómo ejecutar tareas con la misma función y distintos parámetros?
Se utiliza el parámetro `pvParameters` de la función `xTaskCreate()`. Este puntero `void*` permite pasar cualquier estructura de datos a la tarea al momento de su creación.

### ¿Qué pasa cuando una cola se llena?
Si se intenta insertar un elemento en una cola llena, la tarea se bloqueará durante el tiempo especificado en `xTicksToWait`. Si el tiempo expira y la cola sigue llena, la función devolverá un error (`errQUEUE_FULL`).

### Deadlock: Ejemplo y Explicación
Un **Deadlock** ocurre cuando dos tareas se bloquean mutuamente esperando un recurso que la otra posee.
- **Ejemplo:** Tarea A toma Mutex 1 y espera Mutex 2. Al mismo tiempo, Tarea B toma Mutex 2 y espera Mutex 1. Ninguna puede avanzar.
- **Prevención:** Establecer una jerarquía de adquisición de recursos (siempre pedir 1 y luego 2) o usar tiempos de espera (*timeouts*).

---
*Desarrollado por: Juan Felipe Cárdenas y Equipo - 2026*
