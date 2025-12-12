#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Ticker.h>

// --------- KONFIGURATION ---------
const char* wifi_ssid = ""; // Bibliotekets Wi-Fi
const char* wifi_pass = "";                 // Tomt lösenord för öppet Wi-Fi

ESP8266WebServer server(80);

// Historik (10 minuter @ 10 sekunders intervall)
const int HISTORY_SIZE = 60;
struct NetworkData {
  String ssid;
  int rssi;
  int channel;
  String bssid;
  String encryption;
};

NetworkData history[HISTORY_SIZE];
int histIndex = 0;

// Ticker scanTimer;  // Kommentera tills stabilitet testad
unsigned long startTime;

// Konvertera encryption
String encryptionTypeToStr(uint8_t type) {
  switch(type){
    case ENC_TYPE_NONE: return "Open";
    case ENC_TYPE_WEP: return "WEP";
    case ENC_TYPE_TKIP: return "WPA/TKIP";
    case ENC_TYPE_CCMP: return "WPA2/AES";
    case ENC_TYPE_AUTO: return "Auto";
    default: return "Unknown";
  }
}

// Lagra historik
void addToHistory(String ssid, int rssi, int channel, String bssid, String encryption) {
  history[histIndex].ssid = ssid;
  history[histIndex].rssi = rssi;
  history[histIndex].channel = channel;
  history[histIndex].bssid = bssid;
  history[histIndex].encryption = encryption;

  histIndex = (histIndex + 1) % HISTORY_SIZE;
}

// Skanna WiFi
void scanWiFi() {
  int n = WiFi.scanNetworks();
  if (n == 0) return;

  int best = 0;
  for (int i = 1; i < n; i++) {
    if (WiFi.RSSI(i) > WiFi.RSSI(best)) best = i;
  }

  addToHistory(
    WiFi.SSID(best),
    WiFi.RSSI(best),
    WiFi.channel(best),
    WiFi.BSSIDstr(best),
    encryptionTypeToStr(WiFi.encryptionType(best))
  );

  WiFi.scanDelete();
}

// Webbgränssnitt
void handleRoot() {
  String html =
    "<h1>Wi-Fi Analysator</h1>"
    "<p><a href='/latest'>Realtidsdata</a> | "
    "<a href='/history'>Historik</a> | "
    "<a href='/status'>Status</a></p>";
  server.send(200, "text/html", html);
}

void handleLatest() {
  int n = WiFi.scanNetworks();
  String page = "<h2>Senaste skanningen</h2><table border='1'>";
  page += "<tr><th>SSID</th><th>RSSI</th><th>Kanal</th><th>BSSID</th><th>Kryptering</th></tr>";

  for (int i = 0; i < n; i++) {
    page += "<tr><td>" + WiFi.SSID(i) + "</td>";
    page += "<td>" + String(WiFi.RSSI(i)) + "</td>";
    page += "<td>" + String(WiFi.channel(i)) + "</td>";
    page += "<td>" + WiFi.BSSIDstr(i) + "</td>";
    page += "<td>" + encryptionTypeToStr(WiFi.encryptionType(i)) + "</td></tr>";
  }

  page += "</table>";
  server.send(200, "text/html", page);
  WiFi.scanDelete();
}

void handleHistory() {
  String page = "<h2>Historik (10 minuter)</h2><table border='1'>";
  page += "<tr><th>#</th><th>SSID</th><th>RSSI</th><th>Kanal</th><th>BSSID</th><th>Kryptering</th></tr>";

  for (int i = 0; i < HISTORY_SIZE; i++) {
    int idx = (histIndex + i) % HISTORY_SIZE;
    page += "<tr><td>" + String(i+1) + "</td>";
    page += "<td>" + history[idx].ssid + "</td>";
    page += "<td>" + String(history[idx].rssi) + "</td>";
    page += "<td>" + String(history[idx].channel) + "</td>";
    page += "<td>" + history[idx].bssid + "</td>";
    page += "<td>" + history[idx].encryption + "</td></tr>";
  }
  page += "</table>";

  server.send(200, "text/html", page);
}

void handleStatus() {
  unsigned long uptime = (millis() - startTime) / 1000;
  String page = "<h2>Status</h2>";
  page += "<p>Uptime: " + String(uptime) + " sek</p>";
  page += "<p>IP: " + WiFi.localIP().toString() + "</p>";
  page += "<p>Fri heap: " + String(ESP.getFreeHeap()) + "</p>";
  server.send(200, "text/html", page);
}

void setup() {
  Serial.begin(115200);
  Serial.println("Startar ESP...");

  WiFi.mode(WIFI_STA);
  Serial.print("Försöker ansluta till Wi-Fi: ");
  Serial.println(wifi_ssid);

  // Timeout på Wi-Fi
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 15000) { // 15 sek timeout
    delay(500);
    Serial.print(".");
  }

  if(WiFi.status() != WL_CONNECTED){
    Serial.println("\nKunde inte ansluta till Wi-Fi!");
    Serial.println("Startar fallback Access Point...");

    // Starta eget Wi-Fi som Access Point
    WiFi.softAP("ESP_Fallback");
    IPAddress myIP = WiFi.softAPIP();
    Serial.print("Access Point IP: ");
    Serial.println(myIP);
  } else {
    Serial.print("\nWi-Fi anslutet! IP: ");
    Serial.println(WiFi.localIP());
  }

  // Starta webservern
  server.on("/", handleRoot);
  server.on("/latest", handleLatest);
  server.on("/history", handleHistory);
  server.on("/status", handleStatus);
  server.begin();
  Serial.println("Webserver startad!");

  //scanTimer.attach(10, scanWiFi); // Kommentera tills stabilitet testad
  startTime = millis();
}

void loop() {
  server.handleClient();
}
