# W6300-EVA-Pico2_WoL
WIZnetのW6300-EVA-Pico2に増設した押しボタンでWake on LAN (WoL)パケットを送信するArduinoスケッチ

    【概要】
    本スケッチは、W6300-EVB-Pico2（RP2350搭載）を使用し、ネットワーク上の指定した
    PCへWake on LAN (WoL) マジックパケットを送信するためのプログラムです。
    直収押しボタンでWoLパケットを発行し、手元PCを起動します。
    リモートデスクトップを使い始めたので、MQTTブローカーを経由して起動する機能を追加しました。
    遠隔地から使ってみたら、処理時刻を入れたら便利そうに思ったので、時刻処理を追加しました。

    【主な機能】
    1. NTPサーバー (ntp.nict.jp) との時刻同期 (JST: UTC+9)
    2. ボタン操作 (GPIO 14) によるWoL送出
    3. Adafruit IO (MQTT) からの遠隔コマンド受信によるWoL送出
    4. コマンド受領時、Adafruit IOへ日時付きステータス/ACKを返信 (Publish)
       形式: "[YYYY-MM-DD hh:mm:ss] ACK: WoL packet successfully sent"
    5. 約24時間に1回のハードウェア自動再起動 (WDT)
