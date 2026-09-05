
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>

const char* ssid = "Ooredoo-X16-18F58F";
const char* password = "EFC61C5DUr!27";
IPAddress local_IP(192, 168, 0, 184);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(8, 8, 4, 4); 
#define DHTPIN        4
#define DHTTYPE       DHT11
#define PIR_PIN       15
#define HALL_AO_PIN   34
#define BUZZER_PIN    18

#define TEMP_SEUIL_MAX   32.0
#define HUM_SEUIL_MAX    75.0
#define HALL_SEUIL       2500

#define INTERVAL_DHT       3000
#define INTERVAL_SERIAL    1000
#define BUZZER_PIR_ON      150
#define BUZZER_PIR_OFF     150
#define BUZZER_HALL_ON     400
#define BUZZER_HALL_OFF    200
#define BUZZER_TEMP_ON     800
#define BUZZER_TEMP_OFF    800

DHT dht(DHTPIN, DHTTYPE);
WebServer server(80);



float temperature = 0.0;
float humidite    = 0.0;
bool  dhtOK       = false;

bool mouvementDetecte   = false;
bool porteOuverte       = false;
int  hallValeurAnalog   = 0;

unsigned long dernierLectureDHT   = 0;
unsigned long dernierAffichage    = 0;
unsigned long dernierToggleBuzzer = 0;
bool buzzerEtat = false;
int niveauAlarme = 0;

void lireDHT11();
void lirePIR();
void lireHall();
void gererBuzzer();
void afficherEtat();
int  determinerNiveauAlarme();
String texteNiveauAlarme();
String genererPageHTML();
void gererRoutePrincipale();
void gererRouteDonnees();

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(PIR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  dht.begin();

  Serial.println();
  Serial.println("=========================================");
  Serial.println(" SMART HOME - ENVIRONNEMENT & SECURITE ");
  Serial.println(" ESP32 + DHT11 + PIR + KY-024 + Buzzer ");
  Serial.println("=========================================");

  Serial.println("Calibration du capteur PIR en cours (30s)...");
  for (int i = 30; i > 0; i--) {
    Serial.print(i);
    Serial.print("s ");
    delay(1000);
  }
  Serial.println("\nPIR pret. Systeme actif.");

  WiFi.begin(ssid, password);
  Serial.print("Connexion au WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("Connecte ! Adresse IP : ");
  Serial.println(WiFi.localIP());

  server.on("/", gererRoutePrincipale);
  server.on("/data", gererRouteDonnees);
  server.begin();
  Serial.println("Serveur web demarre.");
}

void loop() {
  unsigned long maintenant = millis();

  if (maintenant - dernierLectureDHT >= INTERVAL_DHT) {
    dernierLectureDHT = maintenant;
    lireDHT11();
  }

  lirePIR();
  lireHall();
  niveauAlarme = determinerNiveauAlarme();
  gererBuzzer();

  if (maintenant - dernierAffichage >= INTERVAL_SERIAL) {
    dernierAffichage = maintenant;
    afficherEtat();
  }

  server.handleClient();
}

void lireDHT11() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    dhtOK = false;
    Serial.println("[DHT11] Erreur de lecture !");
  } else {
    dhtOK = true;
    temperature = t;
    humidite = h;
  }
}

void lirePIR() {
  mouvementDetecte = (digitalRead(PIR_PIN) == HIGH);
}

void lireHall() {
  hallValeurAnalog = analogRead(HALL_AO_PIN);
  porteOuverte = (hallValeurAnalog >= HALL_SEUIL);
}

int determinerNiveauAlarme() {
  if (dhtOK && (temperature > TEMP_SEUIL_MAX || humidite > HUM_SEUIL_MAX)) {
    return 3;
  }
  if (porteOuverte) {
    return 2;
  }
  if (mouvementDetecte) {
    return 1;
  }
  return 0;
}

void gererBuzzer() {
  unsigned long maintenant = millis();
  unsigned long tOn, tOff;

  switch (niveauAlarme) {
    case 1: tOn = BUZZER_PIR_ON;  tOff = BUZZER_PIR_OFF;  break;
    case 2: tOn = BUZZER_HALL_ON; tOff = BUZZER_HALL_OFF; break;
    case 3: tOn = BUZZER_TEMP_ON; tOff = BUZZER_TEMP_OFF; break;
    default:
      digitalWrite(BUZZER_PIN, LOW);
      buzzerEtat = false;
      return;
  }

  unsigned long duree = buzzerEtat ? tOn : tOff;
  if (maintenant - dernierToggleBuzzer >= duree) {
    dernierToggleBuzzer = maintenant;
    buzzerEtat = !buzzerEtat;
    digitalWrite(BUZZER_PIN, buzzerEtat ? HIGH : LOW);
  }
}

void afficherEtat() {
  Serial.println("-----------------------------------------");
  if (dhtOK) {
    Serial.print("Temperature : "); Serial.print(temperature); Serial.println(" C");
    Serial.print("Humidite    : "); Serial.print(humidite); Serial.println(" %");
  } else {
    Serial.println("DHT11 : donnees indisponibles");
  }

  Serial.print("PIR (mouvement)    : "); Serial.println(mouvementDetecte ? "DETECTE" : "Aucun");
  Serial.print("KY-024 (analogique): "); Serial.println(hallValeurAnalog);
  Serial.print("KY-024 (porte)     : "); Serial.println(porteOuverte ? "OUVERTE" : "Fermee");

  Serial.print("Niveau alarme      : ");
  switch (niveauAlarme) {
    case 0: Serial.println("Aucune - RAS"); break;
    case 1: Serial.println("1 - Mouvement detecte"); break;
    case 2: Serial.println("2 - Porte/fenetre ouverte"); break;
    case 3: Serial.println("3 - Alerte temperature/humidite"); break;
  }
}

String texteNiveauAlarme() {
  switch (niveauAlarme) {
    case 0: return "Systeme normal";
    case 1: return "Mouvement detecte";
    case 2: return "Porte ou fenetre ouverte";
    case 3: return "Alerte temperature ou humidite";
  }
  return "Inconnu";
}

String genererPageHTML() {
  String page = "<!DOCTYPE html><html lang='fr'><head><meta charset='UTF-8'>";
  page += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  page += "<title>Smart Home - Environnement et Securite</title>";
  page += "<style>";
  page += "body{font-family:Segoe UI,Arial,sans-serif;background:#0f1724;color:#e6edf3;margin:0;padding:30px;}";
  page += "h1{text-align:center;color:#4fd1c5;margin-bottom:5px;}";
  page += "p.sub{text-align:center;color:#8b98a5;margin-top:0;margin-bottom:30px;}";
  page += ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:20px;max-width:1000px;margin:0 auto;}";
  page += ".card{background:#161f2e;border-radius:12px;padding:20px;box-shadow:0 4px 10px rgba(0,0,0,0.3);border-left:5px solid #4fd1c5;}";
  page += ".card h2{margin:0 0 10px 0;font-size:16px;color:#9aa7b5;font-weight:500;}";
  page += ".card .valeur{font-size:28px;font-weight:bold;color:#ffffff;}";
  page += ".alerte{border-left-color:#f56565;}";
  page += ".alerte .valeur{color:#f56565;}";
  page += ".statut{max-width:1000px;margin:30px auto 0 auto;padding:18px;border-radius:12px;text-align:center;font-size:18px;font-weight:bold;}";
  page += ".statut.normal{background:#1c3a2e;color:#68d391;}";
  page += ".statut.alarme{background:#3a1c1c;color:#f56565;}";
  page += "</style></head><body>";
  page += "<h1>Smart Home - Environnement et Securite</h1>";
  page += "<p class='sub'>ESP32 - Supervision en temps reel</p>";
  page += "<div class='grid'>";

  page += "<div class='card'><h2>DHT11 - Temperature</h2><div class='valeur'>";
  page += dhtOK ? String(temperature, 1) + " C" : "N/A";
  page += "</div></div>";

  page += "<div class='card'><h2>DHT11 - Humidite</h2><div class='valeur'>";
  page += dhtOK ? String(humidite, 1) + " %" : "N/A";
  page += "</div></div>";

  page += "<div class='card";
  page += mouvementDetecte ? " alerte" : "";
  page += "'><h2>PIR HC-SR501 - Mouvement</h2><div class='valeur'>";
  page += mouvementDetecte ? "Detecte" : "Aucun";
  page += "</div></div>";

  page += "<div class='card";
  page += porteOuverte ? " alerte" : "";
  page += "'><h2>KY-024 - Porte / Fenetre</h2><div class='valeur'>";
  page += porteOuverte ? "Ouverte" : "Fermee";
  page += "</div></div>";

  page += "<div class='card'><h2>KY-024 - Valeur Analogique</h2><div class='valeur'>";
  page += String(hallValeurAnalog);
  page += "</div></div>";

  page += "</div>";

  page += "<div class='statut ";
  page += (niveauAlarme == 0) ? "normal" : "alarme";
  page += "'>";
  page += texteNiveauAlarme();
  page += "</div>";

  page += "<script>setTimeout(function(){location.reload();},2000);</script>";
  page += "</body></html>";

  return page;
}

void gererRoutePrincipale() {
  server.send(200, "text/html", genererPageHTML());
}

void gererRouteDonnees() {
  String json = "{";
  json += "\"temperature\":" + String(dhtOK ? temperature : -1, 1) + ",";
  json += "\"humidite\":" + String(dhtOK ? humidite : -1, 1) + ",";
  json += "\"mouvement\":" + String(mouvementDetecte ? "true" : "false") + ",";
  json += "\"porteOuverte\":" + String(porteOuverte ? "true" : "false") + ",";
  json += "\"hallValeur\":" + String(hallValeurAnalog) + ",";
  json += "\"niveauAlarme\":" + String(niveauAlarme);
  json += "}";
  server.send(200, "application/json", json);
}