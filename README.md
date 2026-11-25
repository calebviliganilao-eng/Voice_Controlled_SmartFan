# ESP32 Smart Fan — Fan speed request diagnostics and relay-step mappings

## Build fix for `sinricpro_fan_send_range_value_event` integer/string mismatch

The preceding release declared an optional `sinricpro_fan_send_range_value_event` with a speculative three-argument signature ending in `int`. It then passed `SINRICPRO_CAUSE_PHYSICAL_INTERACTION`, which is a `const char *`, causing the GCC error:

```
error: passing argument 3 of 'sinricpro_fan_send_range_value_event' makes integer from pointer without a cast
```

**This release removes the unverified event API entirely.** Physical buttons report the selected level through the **documented** `sinricpro_fan_send_power_level_event(device, percentage, cause)` API, so the firmware can compile with the pinned SinricPro ESP-IDF v1.3.0 component. The optional **incoming** `sinricpro_fan_on_range_value` callback is still detected at runtime if the installed library supplies it. The pinned v1.3.0 Fan component may not support incoming `setRangeValue`: the build fix does **not** add missing component support or guarantee Alexa fan range commands work.

The 7 Unity tests validate speed conversion and relay selection, not live cloud/Alexa transport. For actual 3-speed motor RPM on a two-wire fan, use a suitable PWM motor driver; the relays here select lab outputs only.

### Existing project quick fix

If your prior project folder is `C:\ESP32Projects\SmartFanRangeFix`, run `APPLY_COMPILE_FIX.ps1` from this new extracted project folder and supply that path as `-Target`. It backs up the target's `src/main.c` and replaces **only that source file**, preserving existing `src/app_secrets.h`, PlatformIO configuration, and other user files.

```powershell
powershell -ExecutionPolicy Bypass -File .\APPLY_COMPILE_FIX.ps1 -Target "C:\ESP32Projects\SmartFanRangeFix"
cd "C:\ESP32Projects\SmartFanRangeFix"
pio test
pio check -e sinric_lab --skip-packages
pio run -e sinric_lab
```


This builds on `SmartFan_SPEED_1_2_3_ALEXA_FIXED`. It retains the ESP-IDF/PlatformIO configuration, ESP32 GPIO wiring, two ESP32 environments, 2 MB flash layout, Wi-Fi connection, and physical buttons. It **does not** contain Wi-Fi or Sinric Pro credentials.

## Why On/Off can work while fan level gives "offline" or "Device did not handle request"

Sinric Pro has **different actions** for Fan speed:
- **`setRangeValue`** — typical **Fan/FanUS level 1, 2, 3** action; official sample messages document `setRangeValue` for Fan.
- **`setPowerLevel`** — percentage capability (`0...100`), used by the ESP-IDF component's **Fan example** and the original firmware.

A request whose action is `setRangeValue` **does not trigger** an `on_power_level` callback, even if that callback correctly maps numbers 1→33%, 2→66%, 3→100%. This is why **the previous "1/2/3 fix" did not fix remote commands for some Sinric Fan devices**.

This revision **optionally registers** a Fan `setRangeValue` handler if your installed Sinric Pro ESP-IDF component actually exports `sinricpro_fan_on_range_value`. The declaration is weak, so it builds with version 1.3.0 even when that symbol is absent. It also retains `setPowerLevel` and `adjustPowerLevel` callbacks. When the component lacks Range support, the firmware **prints an explicit warning** at boot rather than claiming the speed command is supported.

### If the boot log says `lacks setRangeValue Fan API`

This is a **library/capability mismatch**, not a numeric mapping problem. **You must use a compatible Sinric Pro RangeController implementation, or create a Sinric Pro Device Template with Power State + Power Level (0–100) capabilities**, then register/update the fan device in the portal and update its matching Device ID/credentials in `src/app_secrets.h`. Your new power-level-capable device can receive `setPowerLevel` requests handled here. The old Fan level-1/2/3 UI will still send `setRangeValue` until its device type/template changes.

To inspect the actual component headers after build:

```powershell
powershell -ExecutionPolicy Bypass -File .\CHECK_SINRIC_SPEED_API.ps1
```

### Exact mappings in firmware

| Range or percentage input | Logical speed | Relay in lab mode |
| --- | --- | --- |
| 0 | Off | none |
| 1 or 33 | Low, 33% | GPIO32 / Module A IN1 |
| 2 or 66 | Medium, 66% | GPIO33 / Module A IN2 |
| 3 or 100 | High, 100% | GPIO25 / Module B IN1 |

The PowerLevel and optional Range callback both use these mappings. `on_range_value` returns a step `1,2,3`, while `on_power_level` returns the resulting `33/66/100` percent as required by its protocol. Physical buttons send `PowerLevelEvent` with `33,66,100` percent. The unverified Range event sender is intentionally not used.

## Alexa voice commands

If your Sinric device exposes a proper **Range** capability, try:
- “Alexa, set Smart Fan speed to 1.”
- “Alexa, set Smart Fan speed to 2.”
- “Alexa, set Smart Fan speed to 3.”

If your device exposes **Power Level** instead, try:
- “Alexa, set the power on Smart Fan to 33 percent.”
- “Alexa, set the power on Smart Fan to 66 percent.”
- “Alexa, set the power on Smart Fan to 100 percent.”

These phrases depend on the actual Sinric Pro device's **Alexa capability**. Firmware cannot parse arbitrary Alexa wording; Alexa interprets the voice request and sends an action supported by the linked device. After changing capabilities, disable/re-enable the Sinric Pro skill or remove and rediscover the old fan in the Alexa app if it still reports offline.

**If the serial monitor shows `Signature verification failed` instead of a `REMOTE` callback, check that the device uses the current App Key + Secret and that the connected Alexa skill is linked to the correct Sinric account.** Firmware cannot skip authentication.

## Real two-wire DC fan hardware limitation

Three relays alone **do not** create three real motor speeds for a two-wire DC fan. `sinric_lab` switches three independent relay outputs as a LAB DEMONSTRATION. `sinric_dc` turns the same fan ON/OFF using Module A Channel 1 for all nonzero logical levels. To change actual RPM, add a motor-driver/PWM speed controller rated for your DC motor and use appropriate PWM-capable firmware. Never connect a motor directly to an ESP32 GPIO.

## How to build

Extract to a folder without spaces, e.g. `C:\ESP32Projects\SmartFan_RANGE_POWERLEVEL_DIAGNOSTIC_FIX`, and open that exact folder in VS Code:

```powershell
Copy-Item .\src\app_secrets.h.example .\src\app_secrets.h
notepad .\src\app_secrets.h
pio project config
pio test
pio check -e sinric_lab --skip-packages
pio run -e sinric_lab
powershell -ExecutionPolicy Bypass -File .\CHECK_SINRIC_SPEED_API.ps1
```

Do not paste live credentials into chat. Close Serial Monitor before upload:

```powershell
powershell -ExecutionPolicy Bypass -File .\UPLOAD_FAN.ps1 -Environment sinric_lab
```

After a successful upload, open a monitor on the correct port. Test portal speed changes and watch for `REMOTE RANGE setRangeValue` or `REMOTE POWERLEVEL setPowerLevel`. If neither appears but On/Off does, verify the Sinric device's Range/PowerLevel capability matches what this component can handle.

## Verification

The relay mapping and both input representations are unit-tested with a host C compiler. C syntax is checked with ESP-IDF interface stubs. Full Windows PlatformIO, Cloud, and Alexa integration **cannot** be verified in this environment. The optional Range callback is only registered if the installed library actually provides it.
