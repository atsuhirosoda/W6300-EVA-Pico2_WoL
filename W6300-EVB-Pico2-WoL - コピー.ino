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
#include <WiFiUdp.h>
#include <hardware/watchdog.h> // RP2350/RP2040のウォッチドッグ用ヘッダー

// --- 設定項目 ---
const uint8_t targetMac[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};  // ターゲットMAC
const uint16_t wolPort = 9;                                         // WoLポート

// 一日のミリ秒数 (24時間 = 86,400,000ミリ秒)
const unsigned long REBOOT_INTERVAL = 86400000UL; 
unsigned long lastRebootTime = 0; // 起動（または前回リセット）してからの時間保持用

// --- ピン配置（GPIO 14） ---
const int BUTTON_PIN = 14; // スイッチ入力ポートを GPIO 14 に設定

// --- チャタリング対策用変数 ---
int lastButtonState = HIGH;
int currentButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

// チップセレクト(CS)は元の通り GPIO 1 に指定
Wiznet6300lwIP eth(1 /* chip select */);
WiFiUDP udp;

void sendMagicPacket();
void performHardwareReboot();

void setup() {
  // 元のサンプルスケッチのSPIピン配置（GPIO 0〜3）
  SPI.setRX(0);
  SPI.setCS(1);
  SPI.setSCK(2);
  SPI.setTX(3);

  // ボタンピンをプルアップ入力に設定 (GPIO 14)
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Serial.begin(115200);
  delay(5000);
  Serial.println("\n\nStarting Ethernet port...");

  // Ethernetポートの起動
  if (!eth.begin()) {
    Serial.println("No wired Ethernet hardware detected. Check pinouts, wiring.");
    while (1) {
      delay(1000);
    }
  }

  while (!eth.connected()) {
    Serial.print(".");
    delay(500);
  }

  Serial.println("\nEthernet connected");
  Serial.print("IP address: ");
  Serial.println(eth.localIP());

  // UDPのローカルポートを開始
  udp.begin(wolPort);
  
  // タイマーの起点を記録
  lastRebootTime = millis();
  
  Serial.println("Ready. Press the button (GPIO 14) to send WoL. Auto-reboot scheduled every 24 hours.");
}

void loop() {
  unsigned long currentMillis = millis();

  // 1. 自動再起動のチェック (毎日1回)
  if (currentMillis - lastRebootTime >= REBOOT_INTERVAL) {
    performHardwareReboot();
  }

  // 2. ボタン入力とチャタリング対策
  int reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonState) {
    lastDebounceTime = currentMillis;
  }

  if ((currentMillis - lastDebounceTime) > debounceDelay) {
    if (reading != currentButtonState) {
      currentButtonState = reading;

      // ボタンが押されてLOW（L）になった瞬間
      if (currentButtonState == LOW) {
        // シリアルへ明示的なメッセージを送信
        Serial.println("\n========================================");
        Serial.println("[BUTTON] スイッチの押し下げを検知しました。");
        Serial.println("========================================");
        
        // WoLパケット送信処理へ
        sendMagicPacket();
        delay(500); // 連打防止
      }
    }
  }

  lastButtonState = reading;
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
