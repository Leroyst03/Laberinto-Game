# 🌀 Laberinto ESP32

Juego de laberinto para **ESP32 + pantalla OLED 128x64** controlado con joystick analógico. El objetivo es sobrevivir el mayor tiempo posible mientras un enemigo te persigue por el laberinto usando el algoritmo **BFS (Breadth-First Search)** para encontrar siempre la ruta más corta hacia ti.

---

## 🎮 ¿En qué consiste el juego?

Eres el punto sólido **(A)** que aparece en la esquina superior izquierda del laberinto. Un enemigo **(P)**, representado como un cuadrado hueco, comienza en la esquina inferior derecha y te persigue sin descanso.

- Muévete por los pasillos del laberinto usando el joystick
- El enemigo recalcula cada ciclo el camino más corto hacia ti mediante BFS
- **Si el enemigo llega a tu misma celda, pierdes**
- Al perder se muestra el tiempo sobrevivido y el nivel alcanzado

### Dificultad progresiva

Cada **30 segundos** el enemigo se vuelve más rápido. La velocidad del HUD muestra el nivel actual en tiempo real.

| Tiempo | Nivel | Velocidad enemigo |
|--------|-------|-------------------|
| 0 – 29s | Lv1 | 480 ms/paso |
| 30 – 59s | Lv2 | 430 ms/paso |
| 60 – 89s | Lv3 | 380 ms/paso |
| 90 – 119s | Lv4 | 330 ms/paso |
| 120s+ | Lv5+ | ... hasta 80 ms/paso |

---

## 🗺️ El laberinto

El mapa es una cuadrícula de **32 × 16 celdas** (cada celda = 4×4 píxeles, cubriendo la pantalla completa de 128×64 px).

Fue generado con el algoritmo **Recursive Backtracker DFS** y posteriormente se rompió el **40% de las paredes internas elegibles** para crear rutas alternativas y hacer el juego escapable.

- ✅ 261 celdas libres, 261 alcanzables — **conectividad 100%**
- ✅ 126 intersecciones con 3 o más salidas — **múltiples rutas de escape**
- ✅ Spawn jugador `(1, 1)` y enemigo `(29, 13)` verificados como celdas libres

---

## 🔧 Hardware necesario

| Componente | Descripción |
|-----------|-------------|
| ESP32 | Cualquier placa ESP32 dev board |
| Pantalla OLED | 128×64 px, controlador SSD1306, protocolo I2C |
| Joystick | Módulo joystick analógico con botón (tipo KY-023) |
| Cables | Dupont macho-hembra |

---

## 🔌 Conexiones

### OLED → ESP32

| OLED | ESP32 |
|------|-------|
| SDA | GPIO 21 |
| SCL | GPIO 22 |
| VCC | 3.3V |
| GND | GND |

### Joystick → ESP32

| Joystick | ESP32 | Nota |
|----------|-------|------|
| VCC | 3.3V | |
| GND | GND | |
| VRX | GPIO 34 | ADC solo entrada |
| VRY | GPIO 35 | ADC solo entrada |
| SW | GPIO 32 | Botón reiniciar |

> **Nota:** Los pines GPIO 34 y 35 del ESP32 son de solo entrada analógica (input-only), ideales para el joystick ya que no necesitan ser configurados como salida.

---

## 💻 Instalación con PlatformIO (VSCode)

1. Clona o descarga este repositorio
2. Abre la carpeta del proyecto en VSCode: `File → Open Folder`
3. PlatformIO detecta automáticamente el `platformio.ini`
4. Conecta el ESP32 por USB
5. Haz clic en **Upload** (→) en la barra inferior de PlatformIO

Las dependencias se instalan automáticamente:
```
adafruit/Adafruit SSD1306 @ ^2.5.7
adafruit/Adafruit GFX Library @ ^1.11.9
```

---

## 📁 Estructura del proyecto

```
maze_game/
├── platformio.ini      # Configuración de la placa y librerías
└── src/
    └── main.cpp        # Código fuente completo del juego
```

---

## ⚙️ Parámetros ajustables

Todas las constantes de dificultad están agrupadas al inicio de `main.cpp`:

```cpp
#define PLAYER_SPEED     150UL   // ms por paso del jugador
#define ENEMY_SPEED_BASE 480.0f  // ms por paso inicial del enemigo
#define ENEMY_SPEED_MIN   80.0f  // velocidad máxima del enemigo (piso)
#define SPEED_INTERVAL    30UL   // segundos entre cada aumento
#define SPEED_STEP        50.0f  // ms que se restan por intervalo
```

---

## 🧠 Cómo funciona el BFS

Cada vez que el enemigo se mueve, ejecuta un **BFS completo** desde su posición hasta la tuya sobre el grid de 32×16. El camino se reconstruye hacia atrás usando un array de padres (`bfsPar`) desde el destino hasta el origen, obteniendo el primer paso óptimo a dar.

```
Enemigo (P) ──BFS──▶ calcula ruta más corta ──▶ avanza un paso
                      recalcula cada ENEMY_SPEED ms
```

Como el BFS garantiza el camino más corto, no puedes simplemente correr en línea recta — tienes que usar las intersecciones del laberinto para crear distancia y confundirlo.

---

## 🕹️ Controles

| Acción | Control |
|--------|---------|
| Mover jugador | Joystick (4 direcciones) |
| Reiniciar (tras Game Over) | Botón SW del joystick |
| Iniciar partida (pantalla título) | Botón SW del joystick |
