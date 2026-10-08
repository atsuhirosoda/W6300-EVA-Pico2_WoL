/*
    W6300-EVB-Pico2 有線LAN通信 Wake on LAN (WoL) 送出スケッチ 
    + Adafruit IO MQTT連携 & NTP時刻付与返信(ACK)機能

    【主な機能】
    1. NTPサーバー (ntp.nict.jp) との時刻同期 (JST: UTC+9)
    2. ボタン操作 (GPIO 14) によるWoL送出
    3. Adafruit IO (MQTT) からの遠隔コマンド受信によるWoL送出
    4. コマンド受領時、Adafruit IOへ日時付きステータス/ACKを返信 (Publish)
       形式: "[YYYY-MM-DD hh:mm:ss] ACK: WoL packet successfully sent"
    5. 約24時間に1回のハードウェア自動再起動 (WDT)
*/

#include <W6300lwIP.h>
#include <WiFiClientSecure.h> // lwIP上で動作するTLSクライアント
#include <WiFiUdp.h>
#include <PubSubClient.h>     // Arduinoライブラリマネージャーから導入
#include <hardware/watchdog.h>
#include <time.h> // 標準C時刻関数

// --------------------------------------------------
// --- WoL設定 ---
// --------------------------------------------------
const uint8_t targetMac[6] = {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc}; // ターゲットMAC
const uint16_t wolPort = 9;                                         // WoLポート

// Adafruit IO 認証設定
const char* IO_SERVER      = "YOUR MQTT SERVER URL";
const int   IO_PORT        = 8883;                     // MQTTS (TLS暗号化)
const char* IO_USERNAME    = "YOUR MQTT USERNAME"; // 要変更
const char* IO_KEY         = "YOUR MQTT KEY FLASE";      // 要変更
const char* FEED_NAME_CMD  = "test-wol-cmd";         // Adafruit IO上で作成したFeed名
const char* FEED_NAME_STAT = "test-wol-status";             // 応答・状態返信用Feed名

// 購読トピック (Sub): {username}/feeds/{feed_name}
String subscribeTopic = String(IO_USERNAME) + "/feeds/" + String(FEED_NAME_CMD);
// 返信トピック (Pub): {username}/feeds/{feed_name}
String publishStatusTopic = String(IO_USERNAME) + "/feeds/" + String(FEED_NAME_STAT);

// NTP設定 (日本標準時: UTC + 9時間 = 32400秒, 夏時間なし: 0)
const long  GMT_OFFSET_SEC = 9 * 3600;
const int   DAYLIGHT_OFFSET_SEC = 0;
const char* NTP_SERVER1 = "ntp.nict.jp";
const char* NTP_SERVER2 = "time.google.com";

// 一日のミリ秒数 (24時間 = 86,400,000ミリ秒)
const unsigned long REBOOT_INTERVAL = 86400000UL; 
unsigned long lastRebootTime = 0;

// MQTT再接続の間隔制御用 (ノンブロッキング)
unsigned long lastMqttReconnectAttempt = 0;
const unsigned long MQTT_RECONNECT_INTERVAL = 5000;

// --- ピン配置（GPIO 14） ---
const int BUTTON_PIN = 14;
const int W6300_RST  = 20; // W6300-EVB-Pico2のリセットピン (GP20)

// --- チャタリング対策用変数 ---
int lastButtonState = HIGH;
int currentButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

// チップセレクト(CS)は GPIO 16
Wiznet6300lwIP eth(16 /* chip select */);
WiFiUDP udp;
WiFiClientSecure tlsClient;
PubSubClient mqttClient(tlsClient);

// プロトタイプ宣言
void sendMagicPacket();
void performHardwareReboot();
void mqttCallback(char* topic, byte* payload, unsigned int length);
boolean reconnectMQTT();
void sendMqttStatus(const char* statusMsg);
String getTimestampString();
void syncNtpTime();

// --------------------------------------------------
// 初期設定 (setup)
// --------------------------------------------------
void setup() {
  // --------------------------------------------------
  // 【最重要】W6300 のハードウェアリセット解除処理
  // --------------------------------------------------
  pinMode(22, OUTPUT);
  digitalWrite(22, LOW);   // 一旦リセット状態にする
  delay(50);
  digitalWrite(22, HIGH);  // リセット解除 (アクティブLow)
  delay(200);              // W6300内部クロック・PHYの安定待ち

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // W6300 ハードウェアリセット制御 (確実にリセット解除する)
  pinMode(W6300_RST, OUTPUT);
  digitalWrite(W6300_RST, LOW);
  delay(10);
  digitalWrite(W6300_RST, HIGH);
  delay(100);


  Serial.begin(115200);
  delay(2000);
  // Ethernetポートの起動
  delay(1000);
  Serial.println("\n\nStarting Ethernet (W6300 via QSPI)");

  // ここでチップ認識が行われます
  if (!eth.begin()) {
    Serial.println("No wired Ethernet hardware detected. Check pinouts, wiring.");
    while (1) {
      delay(1000);
    }
  }

   // Start the Ethernet port
  Serial.print("Connecting to Ethernet");
  unsigned long dhcpStart = millis();
  while (!eth.connected()) {
    Serial.print(".");
    delay(500);
    // 15秒でDHCPタイムアウト
    if (millis() - dhcpStart > 15000) {
      Serial.println("\nDHCP Timeout! Check cable connection.");
      break;
    }
  }

  if (eth.connected()) {
    Serial.println("\nEthernet connected");
    Serial.print("IP address: ");
    Serial.println(eth.localIP());
  }

  // UDPのローカルポートを開始 (WoL用)
  udp.begin(wolPort);

  // NTP時刻同期の開始
  syncNtpTime();

  // TLS設定 (証明書検証の簡略化)
  tlsClient.setInsecure();

  // MQTTクライアント設定
  mqttClient.setServer(IO_SERVER, IO_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(512);

  lastRebootTime = millis();
  Serial.println("Ready. Ready for Button or MQTT triggers.");
}

// --------------------------------------------------
// メインループ (loop)
// --------------------------------------------------
void loop() {
  unsigned long currentMillis = millis();

  // 1. 自動再起動のチェック (毎日1回)
  if (currentMillis - lastRebootTime >= REBOOT_INTERVAL) {
    performHardwareReboot();
  }

  // 2. MQTT接続の維持・処理
  if (!mqttClient.connected()) {
    if (currentMillis - lastMqttReconnectAttempt >= MQTT_RECONNECT_INTERVAL) {
      lastMqttReconnectAttempt = currentMillis;
      if (reconnectMQTT()) {
        lastMqttReconnectAttempt = 0;
      }
    }
  } else {
    mqttClient.loop();
  }

  // 3. ボタン入力とチャタリング対策
  int reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonState) {
    lastDebounceTime = currentMillis;
  }

  if ((currentMillis - lastDebounceTime) > debounceDelay) {
    if (reading != currentButtonState) {
      currentButtonState = reading;

      if (currentButtonState == LOW) {
        Serial.println("\n========================================");
        Serial.println("[BUTTON] スイッチの押し下げを検知しました。");
        Serial.println("========================================");
        
        sendMagicPacket();
        sendMqttStatus("LOCAL_BUTTON: WoL packet sent");
        delay(500); // 連打防止
      }
    }
  }

  lastButtonState = reading;
}

// --------------------------------------------------
// NTP時刻同期処理
// --------------------------------------------------
void syncNtpTime() {
  Serial.println("[NTP] SNTP時刻同期を開始します(JST)...");
  // タイムゾーンを日本標準時 (UTC+9、夏時間なし) に明示設定
  setenv("TZ", "JST-9", 1);
  tzset();
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER1, NTP_SERVER2);

  // 初回時刻取得の待機 (最大10秒)
  time_t now = time(nullptr);
  int retry = 0;
  while (now < 100000 && retry < 20) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
    retry++;
  }
  Serial.println();

  if (now >= 100000) {
    Serial.print("[NTP] 同期完了: ");
    Serial.println(getTimestampString());
  } else {
    Serial.println("[NTP] 警告: 時刻同期がタイムアウトしました (バックグラウンドで再試行されます)");
  }
}

// --------------------------------------------------
// 日時文字列の取得関数 [YYYY-MM-DD hh:mm:ss]
// --------------------------------------------------
String getTimestampString() {
  time_t now = time(nullptr);
  struct tm timeinfo;
  
  // 有効な時刻が取れていない場合のフォールバック
  if (now < 100000) {
    return "[TIME_UNSYNC]";
  }

  localtime_r(&now, &timeinfo);
  char buf[32];
  strftime(buf, sizeof(buf), "[%Y-%m-%d %H:%M:%S]", &timeinfo);
  return String(buf);
}

// --------------------------------------------------
// MQTT受信コールバック & 返信処理
// --------------------------------------------------
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  message.trim();

  Serial.println("\n========================================");
  Serial.print("[MQTT] 受信トピック: ");
  Serial.println(topic);
  Serial.print("[MQTT] ペイロード: ");
  Serial.println(message);
  Serial.println("========================================");

  // コマンド判定 (WOL, ON, 1, RUN 等に対応)
  if (message.equalsIgnoreCase("WOL") || message.equalsIgnoreCase("ON") || message == "1") {
    Serial.println("[MQTT Command] WoL送出要求を認識しました。");
    
    // 1. WoLマジックパケットを送出
    sendMagicPacket();
    
    // 2. 送信元（Adafruit IO）へ受領・完了通知を返信
    sendMqttStatus("ACK: WoL packet successfully sent");
  } else {
    Serial.println("[MQTT Command] 未知のコマンドのためスキップします。");
    String rejectMsg = "REJECT: Unknown command '" + message + "'";
    sendMqttStatus(rejectMsg.c_str());
  }
}

// --------------------------------------------------
// ステータス・ACK返信関数 (日時文字列を自動付加)
// --------------------------------------------------
void sendMqttStatus(const char* statusMsg) {
  if (mqttClient.connected()) {
    // 日時文字列をプレフィックスとして結合
    String fullMessage = getTimestampString() + " " + String(statusMsg);

    Serial.print("[MQTT] 返信送信中 -> ");
    Serial.print(publishStatusTopic);
    Serial.print(" : ");
    Serial.println(fullMessage);
    
    // ステータスフィードへパブリッシュ
    mqttClient.publish(publishStatusTopic.c_str(), fullMessage.c_str());
  } else {
    Serial.println("[MQTT] 返信失敗 (MQTT未接続)");
  }
}

// --------------------------------------------------
// MQTT非同期再接続
// --------------------------------------------------
boolean reconnectMQTT() {
  Serial.print("[MQTT] Connecting to Adafruit IO...");
  
  String clientId = "W6300-Pico2-" + String(random(0xffff), HEX);
  
  if (mqttClient.connect(clientId.c_str(), IO_USERNAME, IO_KEY)) {
    Serial.println(" Connected!");
    mqttClient.subscribe(subscribeTopic.c_str());
    Serial.print("[MQTT] Subscribed to: ");
    Serial.println(subscribeTopic);
    
    // 起動/再接続時の生存通知 (日時付き)
    sendMqttStatus("ONLINE: RP2350 Ready");
    return true;
  } else {
    Serial.print(" Failed, rc=");
    Serial.println(mqttClient.state());
    return false;
  }
}

// --------------------------------------------------
// WoLマジックパケット送信関数
// --------------------------------------------------
void sendMagicPacket() {
  Serial.println("Preparing Wake on LAN Magic Packet...");

  uint8_t magicPacket[102];
  for (int i = 0; i < 6; i++) magicPacket[i] = 0xFF;
  for (int i = 0; i < 16; i++) {
    for (int j = 0; j < 6; j++) {
      magicPacket[6 + (i * 6) + j] = targetMac[j];
    }
  }

  IPAddress broadcastIp(255, 255, 255, 255);

  Serial.print("Sending WoL packet to ");
  for (int i = 0; i < 6; i++) {
    Serial.print(targetMac[i], HEX);
    if (i < 5) Serial.print(":");
  }
  Serial.println();

  udp.beginPacket(broadcastIp, wolPort);
  udp.write(magicPacket, sizeof(magicPacket));
  udp.endPacket();

  Serial.println("Magic Packet sent successfully.");
}

// --------------------------------------------------
// ウォッチドッグタイマーによる安全なハードウェア再起動
// --------------------------------------------------
void performHardwareReboot() {
  Serial.println("24 hours passed. Performing scheduled hardware reboot...");
  sendMqttStatus("REBOOT: Scheduled daily restart");
  mqttClient.loop(); // 送信完了を待つ
  Serial.flush();
  delay(200);

  watchdog_enable(1, 1);
  while (1) {
    tight_loop_contents();
  }
}
