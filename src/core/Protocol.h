#pragma once
/**
 * BLE JSON protocol v1.
 *
 * Service UUID : 6e400001-b5a3-f393-e0a9-e50e24dcca9e
 * RX (write)   : 6e400002-...   phone -> watch
 * TX (notify)  : 6e400003-...   watch -> phone
 *
 * Все сообщения — UTF-8 JSON-строки, завершаются '\n'.
 * Ответ на команду всегда содержит {"ok":true|false, "id":<echo>}.
 * Асинхронные события от часов: {"ev":"...", ...}
 *
 * --- Команды от телефона ---
 *
 *   {"id":1,"cmd":"ping"}
 *      -> {"id":1,"ok":true,"fw":"1.0","batt":87}
 *
 *   {"id":2,"cmd":"wifi","ssid":"MyNet","pwd":"12345678"}
 *      -> {"id":2,"ok":true}
 *
 *   {"id":3,"cmd":"wifi_off"}
 *      -> {"id":3,"ok":true}
 *
 *   {"id":4,"cmd":"bt","on":false}      // BLE обычно уже включён для этой команды
 *      -> {"id":4,"ok":true}
 *
 *   {"id":5,"cmd":"city","name":"Moscow","lat":55.75,"lon":37.62}
 *      -> {"id":5,"ok":true}
 *
 *   {"id":6,"cmd":"brightness","v":128} // 0..255
 *      -> {"id":6,"ok":true}
 *
 *   {"id":7,"cmd":"timeout","s":15}     // 0 = никогда
 *      -> {"id":7,"ok":true}
 *
 *   {"id":8,"cmd":"alarm_set","idx":0,"h":7,"m":30,"en":true,"days":0x1F}
 *      -> {"id":8,"ok":true}
 *
 *   {"id":9,"cmd":"alarm_get"}
 *      -> {"id":9,"ok":true,"alarms":[
 *             {"idx":0,"h":7,"m":30,"en":true,"days":31}, ... ]}
 *
 *   {"id":10,"cmd":"notify","app":"Telegram","title":"Ivan","body":"Hi"}
 *      -> {"id":10,"ok":true}
 *
 *   {"id":11,"cmd":"steps","days":7}
 *      -> {"id":11,"ok":true,"days":[
 *             {"ymd":"2026-10-01","steps":8432}, ... ]}
 *
 *   {"id":12,"cmd":"training_list"}
 *      -> {"id":12,"ok":true,"items":[
 *             {"id":1,"type":"walk","start":1730000000,"dur":1800,"steps":2400,"dist":1800}, ... ]}
 *
 *   {"id":13,"cmd":"training_clear","id":3}      // id=0 => стереть все
 *      -> {"id":13,"ok":true}
 *
 *   {"id":14,"cmd":"time_sync","unix":1730000000}
 *      -> {"id":14,"ok":true}
 *
 *     {"id":N,"cmd":"survival","on":true}
 *      -> {"id":N,"ok":true}
 *         После ответа часы сохраняют флаг в NVS и перезагружаются в режим
 *         выживания: экран/LoRa/BLE/WiFi/IR/тач не работают, МК лежит в deep
 *         sleep. Пробуждение — только по боковой кнопке. На каждое нажатие
 *         в Serial уходит строка:
 *           [SURV] boot #K reason=BUTTON cpu=80MHz batt=87% uptime=00:12:45
 *         Двойное нажатие в течение ~1 секунды -> выход, esp_restart() в
 *         нормальный режим. BLE-соединение в этом режиме недоступно,
 *         поэтому удалённо выключить его нельзя — только двойным кликом.
 *
 *   {"id":N,"cmd":"survival","on":false}
 *      -> {"id":N,"ok":true}
 *         На всякий случай — снять флаг до перезагрузки (например, если
 *         предыдущий вызов armAndReboot по какой-то причине не отработал).
 * --- События от часов (notify) ---
 *
 *   {"ev":"step","total":12345}         // раз в минуту, если идут шаги
 *   {"ev":"batt","pct":19,"low":true}   // при переходе через пороги 20/10 %
 *   {"ev":"training_start","type":"walk","ts":1730000000}
 *   {"ev":"training_end","id":5,"steps":2400,"dist":1800,"dur":1800}
 *   {"ev":"alarm","idx":0}
 */

#ifndef FIRMWARE_VERSION_STR
    #define FIRMWARE_VERSION_STR "1.0.0-dev"
#endif

namespace proto {
    static constexpr const char* SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
    static constexpr const char* RX_UUID      = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
    static constexpr const char* TX_UUID      = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";
    static constexpr const char* DEVICE_NAME  = "TWatchS3";
    static constexpr const char* FIRMWARE_VER = FIRMWARE_VERSION_STR;
}