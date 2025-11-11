# TP2 Sistemas Operativos (72.11)

## Integrantes

| Nombre                 | Legajo | Email                      |
|------------------------|--------|----------------------------|
| Jonás Glaubart         | 65790  | jglaubart@itba.edu.ar      |
| Franco Manuel Pampuri  | 65552  | fpampuri@itba.edu.ar       |

---

## Instrucciones de Compilación y Ejecución

### Prerequisitos
- [Docker](https://www.docker.com/products/docker-desktop/) instalado y en ejecución en el sistema

### Compilación
El proyecto soporta dos allocators de memoria: **buddy** (por defecto) y **bitmap**.

```bash
# Compilar con buddy allocator (por defecto)
./compile.sh

# Compilar con bitmap allocator
./compile.sh bitmap

# Compilar con buddy allocator explícitamente
./compile.sh buddy
```

### Ejecución
Después de compilar, ejecutar el sistema operativo con:

```bash
./run.sh
```

---

## Instrucciones de Replicación

### Comandos Disponibles

#### Información del Sistema
- **`help`**: Muestra todos los comandos disponibles
- **`man <comando>`**: Muestra el manual detallado de un comando específico
- **`clear`**: Limpia la pantalla
- **`time`**: Muestra la hora actual del sistema
- **`regs`**: Imprime el último snapshot de registros (capturado con F12)

#### Gestión de Procesos
- **`ps`**: Lista todos los procesos activos con su PID, nombre, prioridad, estado, stack base y si están en foreground
- **`kill <pid>`**: Termina el proceso con el PID especificado
- **`nice <pid> <prioridad>`**: Cambia la prioridad de un proceso (0-5, mayor = más tiempo de CPU)
- **`block <pid>`**: Alterna un proceso entre los estados READY y BLOCKED

#### Gestión de Memoria
- **`mem`**: Muestra estadísticas de memoria (total, usada y disponible)
- **`test_mm <memoria_maxima>`**: Prueba de stress del gestor de memoria asignando y liberando memoria aleatoriamente

#### Comunicación Entre Procesos
- **`cat`**: Lee de stdin y escribe a stdout (útil para probar pipes)
- **`wc`**: Cuenta el número de líneas en stdin
- **`filter`**: Elimina las vocales de stdin
- **`echo <args...>`**: Imprime los argumentos proporcionados a stdout

#### Comandos de Prueba
- **`test_processes <max_procesos>`**: Crea y mata procesos aleatoriamente para probar la gestión de procesos
- **`test_prio <valor_max>`**: Crea procesos con diferentes prioridades para demostrar el scheduling. Crea tres procesos que suman hasta valor_max. Con valores grandes se ve la diferencia debido a las distintas prioridades.
- **`test_sync <iteraciones> <usar_semaforo>`**: Prueba sincronización con o sin semáforos (0=sin sem, 1=con sem)
- **`test_wait_children [cantidad_hijos]`**: Crea procesos hijos y espera a que todos terminen

#### Programas de Demostración
- **`loop <ms>`**: Imprime un mensaje de saludo cada `ms` milisegundos
- **`mvar`**: Proceso interactivo multi-variable (usa semáforos, presionar Ctrl+K para cerrar)
- **`snake`**: Lanza el juego de la serpiente
- **`history`**: Muestra el historial de comandos

#### Prueba de Excepciones
- **`divzero`**: Genera una excepción de división por cero
- **`invop`**: Genera una excepción de código de operación inválido

#### Control de Fuente
- **`font <increase|decrease>`**: Ajusta el tamaño de la fuente
  - `font increase`: Incrementar tamaño de fuente
  - `font decrease`: Disminuir tamaño de fuente

### Caracteres Especiales

#### Pipes
Usar el carácter pipe `|` para redirigir la salida de un comando como entrada de otro:

```bash
# Ejemplo: Hacer echo de texto y contar líneas
echo hello world | wc

# Ejemplo: Filtrar vocales desde cat
cat | filter
```

**Limitación**: La implementación actual soporta solo **un pipe** (máximo 2 comandos en un pipeline).

#### Ejecución en Background
Usar el ampersand `&` al final de un comando para ejecutarlo en background:

```bash
# Ejecutar loop en background
loop 1000 &

# Múltiples procesos en background
loop 500 &
loop 1000 &
ps
```

La shell retornará inmediatamente y se pueden seguir ingresando comandos mientras el proceso en background se ejecuta.

### Atajos de Teclado

- **F12**: Capturar snapshot de registros (ver con comando `regs`)
- **Ctrl+K**: Matar todos los procesos `mvar`
- **Ctrl+C**: Interrumpir ejecución
- **Ctrl+D**: Enviar EOF (End of File) - usar para salir de comandos como `cat`
- **↑ / ↓**: En la terminal, navegar el historial de comandos
- **Backspace**: Borrar el carácter anterior

### Ejemplos de Uso

#### Ejemplo 1: Pipeline Básico
```bash
# Tipear texto a través de cat y filtrar vocales
cat | filter
Hello World
# Salida: Hll Wrld
# Presionar Ctrl+D para salir
```

#### Ejemplo 2: Gestión de Procesos
```bash
# Iniciar procesos en background
loop 1000 &
loop 500 &

# Listar procesos
ps

# Cambiar prioridad (hacer que proc1 se ejecute más seguido)
# la prioridad tiene tres niveles (0, 1, 2) de menor a mayor prioridad.
nice <pid_de_proc1> 2

# Bloquear un proceso temporalmente
block <pid_de_proc2>

# Matar un proceso
kill <pid_de_proc1>
```

#### Ejemplo 3: Pruebas de Memoria
```bash
# Mostrar estadísticas actuales de memoria
mem

# Prueba de stress del gestor de memoria // TODO: (con 128KB máximo)?
test_mm 131072

# Verificar memoria nuevamente
mem
```

#### Ejemplo 4: Pruebas de Sincronización
```bash
# Prueba sin semáforos (esperar condiciones de carrera)
test_sync 1000 0

# Prueba con semáforos (debería estar sincronizado)
test_sync 1000 1
```

#### Ejemplo 5: Scheduling por Prioridad
```bash
# Crear procesos con diferentes prioridades
# Prueba que a mayor prioridad, menor tiempo hasta cumplir tareas, pues se crean 
# procesos con igual prioridad y terminan casi simultaneamente y luego se cambia 
# la prioridad y se observa como van terminando en el orden de las prioridades.
test_prio 100000000

# Obtener el estado de los procesos y sus prioridades
ps
```

#### Ejemplo 6: Creación y Limpieza de Procesos
```bash
# Probar creación y terminación de procesos (es un loop infinito que si hubiera error lo arroja, sino corre infinitamente hasta ser terminado con un CTRL+C)
test_processes 10

# Ver procesos siendo creados y matados
ps
```

---

## Requerimientos Faltantes o Parcialmente Implementados

### Completamente Implementados
- Scheduling de procesos con prioridades (round-robin dentro de cada nivel) y aging simple para evitar starvation entre colas
- Comunicación entre procesos mediante pipes
- Ejecución de procesos en background
- Gestión de memoria con allocators buddy y bitmap
- Semáforos para sincronización
- Bloqueo/desbloqueo de procesos
- Terminación y limpieza de procesos

### Limitaciones
1. **Limitación de Pipes**: Solo soporta **un pipe** (dos procesos máximo en un pipeline). Cadenas más complejas como `cmd1 | cmd2 | cmd3` no están soportadas.

2. **Manejo de Señales**: Soporte limitado de señales (Solo CTRL+C, CTRL+D y CTRL+K para mvar).

4. **Sistema de Archivos**: No hay sistema de archivos persistente.

5. **Ajuste Dinámico de Prioridad**: Solo hay aging por cola para evitar starvation; no existe ajuste más específico basado en uso de CPU de cada proceso.

---

## Limitaciones

- **Máximo de Procesos**: Limitado a 64 procesos concurrentes (`MAX_PROCESSES`)
- **Tamaño de Stack**: Cada proceso tiene un stack fijo de 4KB (`PROCESS_STACK_SIZE`)
- **Buffer de Pipe**: Tamaño de buffer de pipe limitado
- **Allocators de Memoria**:
  - **Buddy**: Heap de 512KB con bloques mínimos de 32 bytes
  - **Bitmap**: Heap de 512KB 
- **Línea de Comandos**: Máximo 1024 caracteres por comando
- **Argumentos**: Máximo 16 argumentos por comando
- **Historial**: Almacena solo los últimos 10 comandos

---

## Citas de Fragmentos de Código y Uso de IA

### Asistencia de IA
Para el trabajo practico utilizamos GitHub Copilot y ChatGPT eventualmente para:
- Debugging de casos edge en los allocators de memoria
- Inicialización de stack para creación de procesos
- Sugerencias generales de código y optimizaciones
- Ayuda a la hora de debuggear algunos errores

### Referencias de Código Externo
- **Buddy Allocator**: Concepto basado en algoritmos estándar de sistema buddy
- **Bitmap Allocator**: Enfoque estándar de asignación basada en bitmap
- **Implementación de Queue**: Cola de prioridad personalizada para scheduling de procesos
- **Bootloader**: Usa Pure64 y BMFS del proyecto BareMetal OS