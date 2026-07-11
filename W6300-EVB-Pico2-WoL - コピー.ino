/*
    W6300-EVB-Pico2 有線LAN通信 Wake on LAN (WoL) 送出スケッチ

    【概要】
    本スケッチは、W6300-EVB-Pico2（RP2350搭載）を使用し、ネットワーク上の指定した
    PCへWake on LAN (WoL) マジックパケットを送信するためのプログラムです。

    【主な機能】
    1. ボタン操作によるWoL送出:
       GPIO 14ピンを内部プルアップ入力に設定しています。このピンをタクトスイッチ等で
       GNDに落とす（Lにする）と、チャタリングを防止しつつ、指定したMACアドレス宛てに
       UDPブロードキャストでマジックパケットを1回送出します。
    2. 定期的な自動再起動:
       システムの長期安定稼働を目的として、millis()タイマーとウォッチドッグタイマー(WDT)を
       組み合わせ、おおむね24時間に1回の頻度でボード自体を自動的にハードウェア再起動します。
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

// --- WoLマジックパケット送信関数 ---
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

// --- ウォッチドッグタイマーによる安全なハードウェア再起動 ---
void performHardwareReboot() {
  Serial.println("24 hours passed. Performing scheduled hardware reboot...");
  Serial.flush();
  delay(100);

  watchdog_enable(1, 1);
  while (1) {
    tight_loop_contents();
  }
}