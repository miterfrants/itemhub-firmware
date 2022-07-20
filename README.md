# How To Contribute

## ino 運作說明 ([API 文件](https://itemhub.io/swagger/index.html))

- setup function 進行 Wifi 連線
- setup function 進行 ItemHub 登入 (`POST` `/v1/oauth/exchange-token-for-device`)
- loop 分成三個部分
  - 通知 ItemHub 裝置狀態為上線 (`POST` `/v1/my/devices/{id}/online`)
  - 檢查 ItemHub 使用者設定開關的狀態, 再將裝置 PIN 的電位改成對應的電位 (`GET` `/v1/my/devices/{id}/switches`)
  - 讀取設定為感測器的 PIN 並把資料送到 ItemHub (`POST` `/v1/my/devices/{id}/sensors/{pin}`)

## Conventions

### 資料夾規範

- MCU 資料夾命名規則, 全小寫, 詞和詞之間用 `-` 隔開
- MCU 資料夾放在 `./`

### ino 檔案須保留由 ItemHub 取代變數

- 定義 Global Wifi 連線資訊變數

```C++
char ssid[] = "{SSID}";
char password[] = "{WIFI_PASSWORD}";
```

- 定義 Global 登入 ItemHub 帳號密碼

```C++
std::string postBody = "{\"clientId\":\"{CLIENT_ID}\",\"clientSecret\":\"{CLIENT_SECRET}\"}";
```

- `setup` function 保留 `{PINS};` 讓 ItemHub Server 取代使用者所設定 PIN

```C++
void setup() {
    ...
    {PINS};
    ...
}
```

## 貢獻 Code 的流程

- fork 一份 repository
- 在自己的 respository 建立一個新的 branch, e.g: `feat/esp8266`, 並修改完 code 以後對 `https://github.com/miterfrants/itemhub-firmware` 這個 repository 開 pull request
- 開 PR 的時候請填寫裝置所有的 PIN 和他對應的 bite

```json
[
  { "name": "TX", "value": 1 },
  { "name": "RX", "value": 3 },
  { "name": "IO0", "value": 0 },
  { "name": "IO2", "value": 2 }
]
```

- ItemHub 團隊會在 code review 後決定是否 merge 與更新到 ItemHub 平台上
