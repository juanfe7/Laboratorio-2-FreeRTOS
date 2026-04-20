# Práctica FreeRTOS: Concurrencia y Arquitectura Técnica

Este repositorio contiene el desarrollo de la práctica de sistemas de tiempo real, enfocada en el uso de tareas, colas (Queues) y semáforos (Mutex) utilizando **FreeRTOS** sobre un ESP32.

---

## 1. Diagrama de la Práctica (FreeRTOS)
El siguiente diagrama detalla la implementación de tareas concurrentes que reutilizan funciones de lectura y escritura, gestionando la comunicación mediante colas y protegiendo el puerto serial con un Mutex.

<div align="center">
  <img width="452" height="283" alt="image" src="https://github.com/user-attachments/assets/2926e599-2758-4350-b91d-0127645a07e4" />
  <p><i>Figura 1: Arquitectura de tareas, colas y sincronización por Mutex para acceso al puerto serial.</i></p>
</div>

### Funcionamiento de la Práctica
- **Reutilización:** Se definieron dos funciones genéricas (Lectura y Escritura). Cada tarea recibe sus parámetros específicos (pines o IDs) al ser creada.
- **Flujo de Datos:** Los datos de los sensores se empaquetan con un *timestamp* y se envían a colas independientes en formato JSON.
- **Acceso Concurrente:** El Mutex garantiza que la escritura en el Monitor Serial sea atómica, evitando que los mensajes de las dos tareas de escritura se mezclen.

---

## 2. Aplicación en Proyecto Final: Arquitectura Hipotética
Se plantea una arquitectura robusta para el **Smart Car ESP32**, aplicando los conceptos de FreeRTOS para gestionar telemetría y control sin bloqueos.

<div align="center">
  <img width="556" height="294" alt="image" src="https://github.com/user-attachments/assets/cf2b53ed-d64d-41af-a4d1-19eb442a1a48" />
  <p><i>Figura 2: Propuesta de arquitectura multi-core y gestión de recursos compartidos para el vehículo.</i></p>
</div>

### Puntos Clave de la Arquitectura
- **Multitarea (Dual Core):** El servidor Web se asigna al Core 1, mientras que el control de motores y telemetría corren en el Core 0.
- **Comunicación Eficiente:** Se utilizan colas para pasar datos desde las interrupciones (ISR) del sensor de velocidad hacia la lógica de cálculo.
- **Integridad de Datos:** Un Mutex protege el "Estado Global" del carro, permitiendo que la interfaz web lea valores consistentes mientras los sensores los actualizan.

---

## 3. Respuestas a Preguntas Técnicas

### ¿Cómo se ejecutan tareas con la misma función pero distintos parámetros?
Una tarea de FreeRTOS recibe siempre un puntero de tipo void* (un puntero genérico).
En el estándar de C, un void* es como una "dirección de memoria universal" que no tiene un tipo de dato asignado. Esto permite que FreeRTOS sea flexible y te deje pasarle cualquier cosa: desde un simple entero hasta una estructura gigante con toda la configuración del carro.

Para usar los datos dentro de la lógica de tu función, debes realizar un casteo de tipo (typecast). Esto le dice al compilador: "Oye, este puntero genérico en realidad apunta a una estructura de este tipo".

### ¿Cuál es el tipo de dato que recibe? y ¿Cómo se convierte al tipo específico?
Se utiliza el parámetro `pvParameters` de la función `xTaskCreate()`. Este puntero genérico `void*` permite pasar una estructura o variable a la tarea. Dentro de la función, se realiza un *casting* al tipo de dato original para acceder a la información.

### ¿Qué pasa cuando una cola se llena?
La tarea que intenta insertar un elemento puede bloquearse durante el tiempo definido en `ticksToWait`. Si el tiempo expira y la cola sigue llena, la función devuelve `errQUEUE_FULL` y el dato se descarta o debe ser manejado por la lógica del programa.

### ¿Es posible que varias tareas lean y escriban en la misma cola?
**Sí.** Las colas en FreeRTOS son *thread-safe* por diseño. Esto permite patrones de muchos productores a un solo consumidor, o viceversa, sin necesidad de implementar semáforos adicionales para proteger la cola.

### ¿Qué es un Deadlock?
Es una condición donde dos o más tareas se bloquean mutuamente esperando recursos que la otra posee. 
- **Ejemplo:** Tarea A tiene el Semáforo 1 y espera el 2; Tarea B tiene el Semáforo 2 y espera el 1. 
- **Solución:** Siempre adquirir los recursos en el mismo orden jerárquico o usar *timeouts*.

---

## 4. Anexos de Código
El código fuente con los ejemplos de Deadlock y Race Conditions se encuentra en:
- `Preguntas_FreeRTOS/Preguntas_FreeRTOS.ino`

---
*Desarrollado por: Juan Felipe Moncada y Juan Felipe Cárdenas - Ingeniería Informática, Universidad de La Sabana, 2026.*
