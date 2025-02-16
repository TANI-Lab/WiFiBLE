# ESP32 WiFi Slave

ESP32を **BLE（Bluetooth Low Energy）デバイス** として動作させ、WiFiの設定を行うシステムです。  
また、SPIスレーブ通信を使用して他のデバイスとデータをやり取りする機能も備えています。

## 📌 機能
- **BLE通信**
  - スマートフォンなどのBLEデバイスからWiFiのSSID・パスワードを設定可能
  - BLE接続時のデバイス管理
- **WiFi接続**
  - BLE経由で受信したSSID・パスワードを使用してWiFiに接続
  - 設定情報をSPIFFSに保存し、再起動後も維持
- **SPIスレーブ通信**
  - `ESP32DMASPISlave` を使用し、高速なSPI通信を実現
  - マスターからのデータ受信・送信処理
- **FreeRTOSマルチタスク**
  - Bluetoothタスク
  - SPI通信タスク
  - WiFi管理タスク

## 📌 動作環境
- **ESP32**
- **Arduino IDE 1.8.19** または **ESP-IDF**
- **ESP32ボードパッケージ 2.0.5**
- **使用ライブラリ**
  - `WiFi.h`
  - `BLEDevice.h`
  - `FS.h` / `SPIFFS.h`
  - `ESP32DMASPISlave.h`
  - `FreeRTOS.h`

## 📌 使い方
### **1. 必要なライブラリのインストール**
Arduino IDEのライブラリマネージャで以下のライブラリをインストール：
- `ESP32 BLE Arduino`
- `ESP32 DMASPI`
- `SPIFFS`
- `WiFi`

### **2. ESP32にスケッチを書き込む**
Arduino IDEを開き、ボード設定を以下のようにする：
- ボード: **ESP32 Dev Module**
- フラッシュサイズ: **4MB (32Mb)**
- SPIFFS: **2MB**
- CPU周波数: **240MHz**
- シリアルボーレート: **115200bps**

### **3. BLEでWiFi設定を送信**
スマートフォンのBLEアプリ（例: **Bluetooth for Ardiono**）を使用して、ESP32にSSIDとパスワードを送信。

- ESP32のBLEデバイス名: **`HAHA-NO-IKARI`**
- **Service UUID:** `55725ac1-066c-48b5-8700-2d9fb3603c5e`
- **Characteristic UUID:** `69ddb59c-d601-4ea4-ba83-44f679a670ba`
- **送信フォーマット:**  
最初に何でもいいのでコマンドを送信します。

Hello!

あとはメッセージに従い情報を入力してください。
設定内容は内部データとして記憶され、次回起動時に自動的に接続されます。

### **4. WiFi接続の確認**
BLEでWiFi情報を送信すると、ESP32がWiFiに接続します。  
成功すると、シリアルモニターに `WiFi接続成功!` と表示されます。

## 📌 注意点
- SPI通信を使用する場合、マスター側の設定も適切に行う必要があります。
- WiFi設定が誤っていると、ESP32がWiFiに接続できません。
- SPIFFSの初回フォーマットには時間がかかることがあります。

## 📌 ライセンス
MIT License

---
