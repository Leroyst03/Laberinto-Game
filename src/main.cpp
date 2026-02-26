/*
 * ============================================================
 *  LABERINTO ESP32  -  OLED 128x64 I2C  +  Joystick
 * ============================================================
 *  CONEXIONES
 *  ──────────────────────────────────────────────────────────
 *  OLED                     ESP32
 *    SDA  ──────────────────  GPIO 21
 *    SCL  ──────────────────  GPIO 22
 *    VCC  ──────────────────  3.3 V
 *    GND  ──────────────────  GND
 *
 *  JOYSTICK                 ESP32
 *    VCC  ──────────────────  3.3 V
 *    GND  ──────────────────  GND
 *    VRX  ──────────────────  GPIO 34  (ADC, solo entrada)
 *    VRY  ──────────────────  GPIO 35  (ADC, solo entrada)
 *    SW   ──────────────────  GPIO 32  (botón – reiniciar)
 *
 *  Grid : 32x16 celdas de 4x4 px (pantalla 128x64)
 *  Laberinto: Recursive Backtracker + 40% de paredes internas
 *  rotas para crear multiples rutas alternativas.
 *  Verificado: 261 libres, 261 alcanzables, 126 intersecciones.
 *  Spawn jugador: (1,1) | Spawn enemigo: (29,13)
 * ============================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ─── Pantalla ─────────────────────────────────────────────
#define SCREEN_W  128
#define SCREEN_H   64
#define OLED_ADDR 0x3C
Adafruit_SSD1306 oled(SCREEN_W, SCREEN_H, &Wire, -1);

// ─── Joystick ─────────────────────────────────────────────
#define PIN_JX  34
#define PIN_JY  35
#define PIN_SW  32
#define DEAD_Z  700
#define JOY_MID 2048

// ─── Grid ─────────────────────────────────────────────────
#define CELL   4
#define COLS   32
#define ROWS   16
#define NCELLS 512

// ─── Timings / Dificultad ──────────────────────────────────
#define PLAYER_SPEED     150UL   // ms por paso del jugador (fijo)
#define ENEMY_SPEED_BASE 480.0f  // ms por paso inicial del enemigo
#define ENEMY_SPEED_MIN   80.0f  // velocidad maxima (piso)
#define SPEED_INTERVAL    30UL   // segundos entre cada aumento de velocidad
#define SPEED_STEP        50.0f  // ms que se restan cada intervalo

struct Pos { int8_t x, y; };

// ─── Mapa ──────────────────────────────────────────────────
// Generado: Recursive Backtracker (seed 77) + 52 paredes rotas extra
// 261 celdas libres | 100% conectado | 126 intersecciones (rutas multiples)
static const char MAZE_TPL[ROWS][COLS + 1] = {
  "11111111111111111111111111111111",
  "10000000001000000000000010100011",
  "10111000101101011101100000000011",
  "10001010100000001000001000001011",
  "10101010101101101010101010001011",
  "10000010001000100010000000001011",
  "10011110100011101010111111111011",
  "10001000100010000000100000000011",
  "11101010100000001011111011100011",
  "10000000000010001000000010100011",
  "10101010011010001000101110101011",
  "10000000000000000000101000001011",
  "10110011101110001101101011111011",
  "10000000000000000000001000000011",
  "11111111111111111111111111111111",
  "11111111111111111111111111111111"
};

char grid[ROWS][COLS];
Pos  player, enemy;

// ─── Estado ────────────────────────────────────────────────
enum State { PLAYING, GAME_OVER };
State         gameState;
unsigned long startTime, surviveTime;

// ─── BFS ───────────────────────────────────────────────────
static uint16_t bfsPar[ROWS][COLS];
static uint16_t bfsQ[NCELLS];
#define NO_VISIT 0xFFFF

inline uint16_t p2i(int x, int y)  { return (uint16_t)(y * COLS + x); }
inline Pos      i2p(uint16_t idx)  { return {(int8_t)(idx % COLS), (int8_t)(idx / COLS)}; }

Pos bfsNextStep(int ex, int ey, int px, int py) {
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++)
      bfsPar[r][c] = NO_VISIT;

  int      head = 0, tail = 0;
  uint16_t srcI = p2i(ex, ey);
  uint16_t dstI = p2i(px, py);

  bfsQ[tail++]   = srcI;
  bfsPar[ey][ex] = srcI;

  const int8_t dx[] = { 1, -1,  0,  0 };
  const int8_t dy[] = { 0,  0,  1, -1 };

  while (head < tail) {
    uint16_t ci = bfsQ[head++];
    if (ci == dstI) break;
    Pos cp = i2p(ci);
    for (int d = 0; d < 4; d++) {
      int nx = cp.x + dx[d];
      int ny = cp.y + dy[d];
      if (nx < 0 || ny < 0 || nx >= COLS || ny >= ROWS) continue;
      if (grid[ny][nx] == '1')        continue;
      if (bfsPar[ny][nx] != NO_VISIT) continue;
      bfsPar[ny][nx] = ci;
      bfsQ[tail++]   = p2i(nx, ny);
    }
  }

  if (bfsPar[py][px] == NO_VISIT) return {-1, -1};

  uint16_t cur = dstI;
  while (true) {
    Pos      cp  = i2p(cur);
    uint16_t par = bfsPar[cp.y][cp.x];
    if (par == srcI) return cp;
    if (par == cur)  break;
    cur = par;
  }
  return {-1, -1};
}

// ─── Init ──────────────────────────────────────────────────
void initGame() {
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++)
      grid[r][c] = MAZE_TPL[r][c];

  player = {1, 1};
  enemy  = {29, 13};
  grid[player.y][player.x] = 'A';
  grid[enemy.y][enemy.x]   = 'P';

  startTime = millis();
  gameState = PLAYING;
}

// ─── Dibujar ───────────────────────────────────────────────
void drawGame() {
  oled.clearDisplay();

  for (int r = 0; r < ROWS; r++) {
    for (int c = 0; c < COLS; c++) {
      char ch = grid[r][c];
      int  px = c * CELL;
      int  py = r * CELL;

      if (ch == '1') {
        oled.fillRect(px, py, CELL, CELL, SSD1306_WHITE);

      } else if (ch == 'A') {
        // Jugador: disco solido 2px
        oled.fillCircle(px + 2, py + 2, 1, SSD1306_WHITE);

      } else if (ch == 'P') {
        // Enemigo: cuadrado hueco para distinguirlo
        oled.drawRect(px, py, CELL, CELL, SSD1306_WHITE);
      }
    }
  }

  // HUD: tiempo y nivel (texto negro sobre borde blanco superior)
  unsigned long t     = (millis() - startTime) / 1000UL;
  unsigned long level = t / SPEED_INTERVAL + 1;
  char buf[16];
  snprintf(buf, sizeof(buf), "%lus Lv%lu", t, level);
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_BLACK);
  oled.setCursor(2, 1);
  oled.print(buf);

  oled.display();
}

void drawGameOver() {
  unsigned long level = (surviveTime / 1000UL) / SPEED_INTERVAL + 1;

  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);

  oled.setTextSize(2);
  oled.setCursor(10, 4);
  oled.print("GAME OVER");

  oled.setTextSize(1);
  oled.setCursor(8, 28);
  oled.print("Tiempo : ");
  oled.print(surviveTime / 1000UL);
  oled.print(".");
  oled.print((surviveTime % 1000UL) / 100UL);
  oled.print("s");

  oled.setCursor(8, 38);
  oled.print("Nivel  : ");
  oled.print(level);

  oled.setCursor(16, 52);
  oled.print("SW = reiniciar");

  oled.display();
}

// ─── Joystick ──────────────────────────────────────────────
Pos readJoystick() {
  int rx = analogRead(PIN_JX) - JOY_MID;
  int ry = analogRead(PIN_JY) - JOY_MID;
  if (abs(rx) > abs(ry)) {
    if (rx >  DEAD_Z) return { 1,  0};
    if (rx < -DEAD_Z) return {-1,  0};
  } else {
    if (ry >  DEAD_Z) return { 0,  1};
    if (ry < -DEAD_Z) return { 0, -1};
  }
  return {0, 0};
}

// ─── Mover jugador ─────────────────────────────────────────
void movePlayer() {
  Pos d = readJoystick();
  if (d.x == 0 && d.y == 0) return;

  int nx = player.x + d.x;
  int ny = player.y + d.y;
  if (nx < 0 || ny < 0 || nx >= COLS || ny >= ROWS) return;
  if (grid[ny][nx] == '1' || grid[ny][nx] == 'P')   return;

  grid[player.y][player.x] = '0';
  player = {(int8_t)nx, (int8_t)ny};
  grid[player.y][player.x] = 'A';
}

// ─── Mover enemigo ─────────────────────────────────────────
void moveEnemy() {
  grid[player.y][player.x] = '0';
  grid[enemy.y][enemy.x]   = '0';

  Pos next = bfsNextStep(enemy.x, enemy.y, player.x, player.y);

  grid[player.y][player.x] = 'A';

  if (next.x == -1) {
    grid[enemy.y][enemy.x] = 'P';
    return;
  }

  grid[enemy.y][enemy.x] = '0';
  enemy = next;
  grid[enemy.y][enemy.x] = 'P';
}

// ─── Setup ─────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("ERROR: OLED no detectada");
    while (true) delay(500);
  }

  pinMode(PIN_SW, INPUT_PULLUP);
  analogReadResolution(12);

  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(2);
  oled.setCursor(12, 6);
  oled.print("LABERINTO");
  oled.setTextSize(1);
  oled.setCursor(10, 30);
  oled.print("Esquiva al enemigo");
  oled.setCursor(14, 42);
  oled.print("SW para jugar");
  oled.display();

  while (digitalRead(PIN_SW) == HIGH) delay(50);
  delay(300);
  initGame();
}

unsigned long lastPlayer = 0, lastEnemy = 0;

// Devuelve el intervalo actual del enemigo en ms según el tiempo sobrevivido
unsigned long enemySpeed() {
  unsigned long seconds = (millis() - startTime) / 1000UL;
  unsigned long level   = seconds / SPEED_INTERVAL;   // 0,1,2,3...
  float speed = ENEMY_SPEED_BASE - (level * SPEED_STEP);
  if (speed < ENEMY_SPEED_MIN) speed = ENEMY_SPEED_MIN;
  return (unsigned long)speed;
}

// ─── Loop ──────────────────────────────────────────────────
void loop() {
  if (gameState == GAME_OVER) {
    drawGameOver();
    if (digitalRead(PIN_SW) == LOW) {
      delay(300);
      initGame();
    }
    return;
  }

  unsigned long now = millis();

  if (now - lastPlayer >= PLAYER_SPEED) {
    movePlayer();
    lastPlayer = now;
  }

  if (now - lastEnemy >= enemySpeed()) {
    moveEnemy();
    lastEnemy = now;
  }

  if (player.x == enemy.x && player.y == enemy.y) {
    surviveTime = millis() - startTime;
    gameState   = GAME_OVER;
    return;
  }

  drawGame();
}