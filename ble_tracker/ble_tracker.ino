#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <vector>

// --- WiFi 設定 --------------------------------
const char* ssid     = "WJH";
const char* password = "a0907863836";

// --- ThingSpeak 設定 --------------------------
const char* TS_API_KEY = "TL3MNCIMRX8H51JW";
const String tsBaseUrl = String("http://api.thingspeak.com/update?api_key=") + TS_API_KEY;

// --- GAS Web App URL --------------------------
const String gasUrlForLine     = "https://script.google.com/macros/s/AKfycbw1_A161Qrkcr0YKpKDYRAv3OQeOXR8ajBjHi-3Ifvx2X-e61WrHVKacETAw2VED9nn/exec";
const String gasUrlForDatabase = "https://script.google.com/macros/s/AKfycbypqO1fwAWmgZ_H_5pVfsA81wwn2RBgfRz9EUqnYBA37jKMVFl779FvBDUxCY0ZbgVJqQ/exec";

// --- BLE & 閾值 --------------------------------
BLEScan* pBLEScan;
const int scanTime      = 10;
const int RSSIThreshold = -80;

// --- 資料結構 --------------------------------
struct Device {
  String mac;
  String name;   // 新增 name 欄
  int    field;
  bool   found;
  int    rssi;
};
std::vector<Device> tracked;

// 快取參數
unsigned long lastFetchTime        = 0;
const unsigned long FETCH_INTERVAL = 30UL * 1000UL;

// --- 抓取 Apps Script 裡的追蹤清單 ----------
void fetchTrackedDevices() {
  Serial.println("==> 開始從 Apps Script 抓取追蹤清單...");
  WiFiClientSecure client; client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, gasUrlForDatabase)) {
    Serial.println("  HTTP begin 失敗");
    return;
  }
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  int code = http.GET();
  Serial.printf("  HTTP GET 回傳碼：%d\n", code);
  if (code == HTTP_CODE_OK) {
    String payload = http.getString();
    DynamicJsonDocument doc(4096);  // 容量
    auto err = deserializeJson(doc, payload);
    if (err) {
      Serial.print("  JSON 解析失敗：");
      Serial.println(err.c_str());
    } else {
      tracked.clear();
      for (auto obj : doc["devices"].as<JsonArray>()) {
        Device d;
        d.mac   = obj["mac"].as<const char*>();
        d.name = obj["name"].as<String>();
        d.field = obj["field"].as<int>();
        d.found = false;
        d.rssi  = -127;
        tracked.push_back(d);
      }
      Serial.printf("  抓到 %d 筆追蹤設定：\n", tracked.size());
      for (auto &d : tracked) {
        Serial.printf("    - MAC: %s, Name: %s, Field: %d\n",
                      d.mac.c_str(), d.name.c_str(), d.field);
      }
    }
  } else {
    Serial.println("  抓取失敗");
  }
  http.end();
}

// --- 更新 ThingSpeak --------------------------
void updateThingSpeak(int fieldId, int value) {
  String url = tsBaseUrl + "&field" + String(fieldId) + "=" + String(value);
  HTTPClient http;
  http.begin(url);
  int code = http.GET();
  Serial.printf("    更新 TS field%d = %d, HTTP %d\n", fieldId, value, code);
  http.end();
}

// --- 呼叫 GAS 推播 LINE (現在帶 mac+name) -----
void notifyToGAS(const String& mac, const String& name) {
  Serial.printf("    呼叫 GAS 通知 MAC=%s, Name=%s 未找到\n",
                mac.c_str(), name.c_str());
  WiFiClientSecure client; client.setInsecure();
  // URL encode name 如果有空白或特殊字元
  String url = gasUrlForLine
             + "?mac="   + mac
             + "&name="  + urlencode(name)
             + "&status=not_found";
  HTTPClient http;
  if (http.begin(client, url)) {
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    int code = http.GET();
    Serial.printf("      GAS 通知回傳 %d\n", code);
    http.end();
  }
}

// (簡單實作) URL encode
String urlencode(const String &s){
  String r;
  char c;
  for (int i=0; i<s.length(); i++){
    c = s.charAt(i);
    if (isalnum(c)) r+=c;
    else if (c==' ') r+='+';
    else {
      char buf[5];
      sprintf(buf,"%%%02X", (uint8_t)c);
      r += buf;
    }
  }
  return r;
}

void setup(){
  Serial.begin(115200);
  Serial.println("=== ESP32 BLE 追蹤 啟動 ===");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid,password);
  Serial.print("連線 WiFi");
  while(WiFi.status()!=WL_CONNECTED){
    delay(500); Serial.print(".");
  }
  Serial.println("\nWiFi 已連線");
  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);

//  fetchTrackedDevices();
  lastFetchTime = millis();
}

void loop(){
  unsigned long now = millis();
  // 每 30s 更新設定
//  if (now-lastFetchTime >= FETCH_INTERVAL){
//    fetchTrackedDevices();
//    lastFetchTime = now;
//  }

  // 重置
  for(auto &d: tracked){
    d.found=false; d.rssi=-127;
  }

  // 每輪都先抓最新設定
  fetchTrackedDevices();
  // BLE 掃描
  Serial.println("--> 開始 BLE 掃描 10s");
  BLEScanResults results = pBLEScan->start(scanTime,false);
  Serial.printf("    掃描結束，共找到 %d 台設備\n", results.getCount());
  for(int i=0;i<results.getCount();i++){
    BLEAdvertisedDevice dev = results.getDevice(i);
    String addr = dev.getAddress().toString().c_str();
    String name = dev.getName().c_str();
    int rssi    = dev.getRSSI();
    Serial.printf("      設備 %02d: %s, 名稱: %s (RSSI=%d)\n",
                  i+1, addr.c_str(),
                  name.length()?name.c_str():"(無名稱)",
                  rssi);
    for(auto &d: tracked){
      if(addr.equalsIgnoreCase(d.mac)){
        d.found=true;
        d.rssi = rssi;
        Serial.printf("        -> 追蹤到 %s (%s), RSSI=%d\n",
                      d.mac.c_str(), d.name.c_str(), rssi);
      }
    }
  }
  pBLEScan->clearResults();

  // 回寫 TS & 通知 LINE
  Serial.println("--> 開始更新 ThingSpeak & 通知");
  for(auto &d: tracked){
    int val = (d.found && d.rssi>=RSSIThreshold)?1:0;
    updateThingSpeak(d.field, val);
    Serial.println("--> ThingSpeak 每次 call api 上傳後，需要 15 秒才能再使用");
    if(!d.found||d.rssi<RSSIThreshold){
      notifyToGAS(d.mac, d.name);
      delay(15666);
    }else{
      delay(15666);
    }
  }

  Serial.println("======================== 本輪結束 google apps script 需等待數秒後才能再抓取 user 指定的裝置 ========================\n");
  delay(10666);
}
