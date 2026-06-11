// ============================================================
//  Monitor de Nível de Caixa d'Água - VERSÃO SERIAL MONITOR
//  ESP32 + Sensor Ultrassônico (HC-SR04 / JSN-SR04T)
//  Três modos de cálculo de volume:
//    CILINDRO   -> exato (precisa do raio).         Ex.: garrafa
//    CONE       -> exato (precisa dos dois raios).  Ex.: caixa medida
//    CAPACIDADE -> estimativa (só precisa dos litros). Ex.: caixa qualquer
// ============================================================

// --- PINOS ---
#define TRIG_PIN 5
#define ECHO_PIN 18

// --- ESCOLHA DO MODO ---
#define CILINDRO   0
#define CONE       1
#define CAPACIDADE 2
const int FORMATO = CILINDRO;   // CILINDRO, CONE ou CAPACIDADE

// --- PARÂMETROS DE INSTALAÇÃO (medidos com o sensor, em cm) ---
// DIST_FUNDO : distância do sensor até o FUNDO (vazio)
// DIST_CHEIO : distância do sensor até a água no NÍVEL MÁXIMO (cheia)
#define DIST_FUNDO  20.0
#define DIST_CHEIO   3.0

// --- MODO CILINDRO (ex.: garrafa) ---
#define RAIO        4.0      // raio interno (cm)

// --- MODO CONE / TRONCO DE CONE (caixa com dimensões medidas) ---
#define RAIO_BASE   30.0     // raio interno da base (cm)
#define RAIO_TOPO   40.0     // raio interno no nível cheio (cm)

// --- MODO CAPACIDADE (caixa qualquer: só os litros estampados) ---
#define CAPACIDADE_L 1000.0  // litros quando cheia

const float PI_VAL = 3.14159;
const float ALTURA_UTIL = DIST_FUNDO - DIST_CHEIO;  // coluna máxima de água

// ------------------------------------------------------------
//  Volume (litros) da água do fundo até a altura h (cm).
// ------------------------------------------------------------
float volumeLitros(float h) {
  if (FORMATO == CILINDRO) {
    return (PI_VAL * RAIO * RAIO * h) / 1000.0;
  } else if (FORMATO == CONE) {
    float r = RAIO_BASE + (RAIO_TOPO - RAIO_BASE) * (h / ALTURA_UTIL);
    return ((PI_VAL * h / 3.0) * (RAIO_BASE * RAIO_BASE + RAIO_BASE * r + r * r)) / 1000.0;
  } else {  // CAPACIDADE: estimativa linear pela altura
    return (h / ALTURA_UTIL) * CAPACIDADE_L;
  }
}

// ------------------------------------------------------------
//  Mede a distância (cm). Retorna -1 se a leitura for inválida.
// ------------------------------------------------------------
float medirDistancia() {
  digitalWrite(TRIG_PIN, LOW);  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duracao = pulseIn(ECHO_PIN, HIGH, 30000);
  float d = (duracao * 0.0343) / 2;

  if (d <= 2 || d > DIST_FUNDO + 5) return -1;
  return d;
}

// ------------------------------------------------------------
//  Faz 15 leituras e devolve a média das válidas (reduz ruído).
// ------------------------------------------------------------
float distanciaMedia() {
  float soma = 0;
  int lidas = 0;
  for (int i = 0; i < 15; i++) {
    float d = medirDistancia();
    if (d != -1) { soma += d; lidas++; }
    delay(20);
  }
  if (lidas == 0) return -1;
  return soma / lidas;
}

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  const char* nome = (FORMATO == CILINDRO) ? "CILINDRO"
                   : (FORMATO == CONE)     ? "CONE" : "CAPACIDADE";

  Serial.println();
  Serial.println("=== Monitor de Nivel de Caixa d'Agua ===");
  Serial.print("Modo: "); Serial.println(nome);
  Serial.print("Altura util da agua: "); Serial.print(ALTURA_UTIL, 1); Serial.println(" cm");
  Serial.print("Capacidade (cheia): "); Serial.print(volumeLitros(ALTURA_UTIL), 2); Serial.println(" L");
  Serial.println("========================================");
}

void loop() {
  float distancia = distanciaMedia();

  if (distancia == -1) {
    Serial.println("[AVISO] Leitura invalida - verifique o sensor.");
    delay(3000);
    return;
  }

  float alturaAgua = DIST_FUNDO - distancia;
  if (alturaAgua < 0) alturaAgua = 0;
  if (alturaAgua > ALTURA_UTIL) alturaAgua = ALTURA_UTIL;

  float volumeL     = volumeLitros(alturaAgua);
  float capacidadeL = volumeLitros(ALTURA_UTIL);
  float percentual  = (volumeL / capacidadeL) * 100.0;
  if (percentual > 100) percentual = 100;
  if (percentual < 0)   percentual = 0;

  Serial.println("----------------------------------------");
  Serial.print("Distancia medida : "); Serial.print(distancia, 1);  Serial.println(" cm");
  Serial.print("Altura da agua   : "); Serial.print(alturaAgua, 1); Serial.println(" cm");
  Serial.print("Nivel            : "); Serial.print(percentual, 1); Serial.println(" %");
  Serial.print("Volume atual     : "); Serial.print(volumeL, 2);    Serial.println(" L");
  Serial.println("----------------------------------------");

  delay(3000);
}
