// ============================================================
//  Monitor de Nível de Caixa d'Água - BLYNK + ALERTAS SMS (Twilio)
//  ESP32 + Sensor Ultrassônico
//  Dashboard no Blynk + SMS quando o nível fica baixo ou cheio
// ============================================================

// --- CONFIGURAÇÕES BLYNK ---
#define BLYNK_TEMPLATE_ID ""
#define BLYNK_TEMPLATE_NAME ""
#define BLYNK_AUTH_TOKEN ""
#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <BlynkSimpleEsp32.h>

// --- WI-FI ---
char ssid[] = "";
char pass[] = "";

// --- CONFIGURAÇÕES TWILIO ---
const char* ACCOUNT_SID    = "";
const char* AUTH_TOKEN     = "";
const char* NUMERO_TWILIO  = "";    // número Twilio (Remetente)
const char* NUMERO_DESTINO = "";  // celular verificado (Destinatario)

// --- PINOS ---
#define TRIG_PIN 5
#define ECHO_PIN 18

// --- ESCOLHA DO MODO ---
#define CILINDRO   0
#define CONE       1
#define CAPACIDADE 2
const int FORMATO = CILINDRO;

// --- PARÂMETROS DE INSTALAÇÃO (cm) ---
#define DIST_FUNDO  12.0
#define DIST_CHEIO   3.5

// --- DIMENSÕES ---
#define RAIO         4.25
#define RAIO_BASE    30.0
#define RAIO_TOPO    40.0
#define CAPACIDADE_L 0.5

// --- LIMIARES DE ALERTA ---
#define NIVEL_BAIXO  30.0   // alerta quando <= 20%
#define NIVEL_CHEIO  98.0   // alerta quando >= 98%
const bool SMS_ATIVO = true;   // true: envia sms, false: para de enviar sms (quando quiser fazer ajustes)

const float PI_VAL = 3.14159;
const float ALTURA_UTIL = DIST_FUNDO - DIST_CHEIO;

BlynkTimer timer;

// Flags pra não mandar SMS repetido
bool alertaBaixoEnviado = false;
bool alertaCheioEnviado = false;

// ------------------------------------------------------------
//  Codifica texto para URL (espaços, +, %, etc.)
// ------------------------------------------------------------
String urlEncode(String str) {
  String encoded = "";
  for (int i = 0; i < str.length(); i++) {
    char c = str.charAt(i);
    if (isalnum(c)) {
      encoded += c;
    } else {
      char code0 = ((c >> 4) & 0xf) + '0';
      if (((c >> 4) & 0xf) > 9) code0 = ((c >> 4) & 0xf) - 10 + 'A';
      char code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9) code1 = (c & 0xf) - 10 + 'A';
      encoded += '%'; encoded += code0; encoded += code1;
    }
  }
  return encoded;
}

// ------------------------------------------------------------
//  Envia um SMS via Twilio.
// ------------------------------------------------------------
void enviarSMS(String mensagem) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Sem WiFi - SMS nao enviado.");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();  // pula validação de certificado (ok p/ protótipo)

  HTTPClient http;
  String url = "https://api.twilio.com/2010-04-01/Accounts/" + String(ACCOUNT_SID) + "/Messages.json";
  http.begin(client, url);
  http.setAuthorization(ACCOUNT_SID, AUTH_TOKEN);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String body = "To=" + urlEncode(NUMERO_DESTINO) +
                "&From=" + urlEncode(NUMERO_TWILIO) +
                "&Body=" + urlEncode(mensagem);

  int code = http.POST(body);
  if (code > 0) {
    Serial.print("SMS -> HTTP "); Serial.println(code);
  } else {
    Serial.print("Falha no SMS: "); Serial.println(http.errorToString(code));
  }
  http.end();
}

// ------------------------------------------------------------
//  Volume (litros) da água até a altura h (cm).
// ------------------------------------------------------------
float volumeLitros(float h) {
  if (FORMATO == CILINDRO) {
    return (PI_VAL * RAIO * RAIO * h) / 1000.0;
  } else if (FORMATO == CONE) {
    float r = RAIO_BASE + (RAIO_TOPO - RAIO_BASE) * (h / ALTURA_UTIL);
    return ((PI_VAL * h / 3.0) * (RAIO_BASE * RAIO_BASE + RAIO_BASE * r + r * r)) / 1000.0;
  } else {
    return (h / ALTURA_UTIL) * CAPACIDADE_L;
  }
}

float medirDistancia() {
  digitalWrite(TRIG_PIN, LOW);  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duracao = pulseIn(ECHO_PIN, HIGH, 30000);
  float d = (duracao * 0.0343) / 2;
  if (d <= 2 || d > DIST_FUNDO + 5) return -1;
  return d;
}

float distanciaMedia() {
  float soma = 0; int lidas = 0;
  for (int i = 0; i < 15; i++) {
    float d = medirDistancia();
    if (d != -1) { soma += d; lidas++; }
    delay(20);
  }
  if (lidas == 0) return -1;
  return soma / lidas;
}

// ------------------------------------------------------------
//  Verifica limiares e dispara SMS apenas na transição.
//  Se SMS_ATIVO for false, apenas registra no Serial (modo teste).
// ------------------------------------------------------------
void verificarAlertas(float percentual) {
  // Nível baixo
  if (percentual <= NIVEL_BAIXO && !alertaBaixoEnviado) {
    if (SMS_ATIVO) {
      enviarSMS("Alerta: caixa d'agua baixa (" + String(percentual, 0) + "%).");
    } else {
      Serial.println(">>> [TESTE] SMS de nivel baixo (nao enviado)");
    }
    alertaBaixoEnviado = true;
  }
  if (percentual > NIVEL_BAIXO + 5) alertaBaixoEnviado = false;  // rearma

  // Nível cheio
  if (percentual >= NIVEL_CHEIO && !alertaCheioEnviado) {
    if (SMS_ATIVO) {
      enviarSMS("Aviso: caixa d'agua cheia (" + String(percentual, 0) + "%).");
    } else {
      Serial.println(">>> [TESTE] SMS de nivel cheio (nao enviado)");
    }
    alertaCheioEnviado = true;
  }
  if (percentual < NIVEL_CHEIO - 5) alertaCheioEnviado = false;  // rearma
}

void enviarDados() {
  float distancia = distanciaMedia();
  if (distancia == -1) {
    Serial.println("[AVISO] Leitura invalida.");
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

  Blynk.virtualWrite(V1, alturaAgua);
  Blynk.virtualWrite(V2, volumeL);
  Blynk.virtualWrite(V3, percentual);

  Serial.print("Altura: "); Serial.print(alturaAgua, 1); Serial.print(" cm | ");
  Serial.print("Volume: "); Serial.print(volumeL, 2);    Serial.print(" L | ");
  Serial.print("Nivel: ");  Serial.print(percentual, 1); Serial.println(" %");

  verificarAlertas(percentual);
}

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  Serial.println("Iniciando Blynk...");
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  timer.setInterval(3000L, enviarDados);
}

void loop() {
  Blynk.run();
  timer.run();
}
