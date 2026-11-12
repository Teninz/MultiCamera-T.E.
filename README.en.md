# MultiCamera T.E.

**Turn up to five Android phones into separate Windows cameras and OBS sources.**

**[Русский](README.md)** · **English**

Wi-Fi, USB, your own Internet relay, optional microphone, stabilization and supported 60 FPS modes.

## Download and install

- **[Windows installer](https://github.com/Teninz/obs-lan-camera/releases/latest/download/MultiCameraTE-Setup.exe)** — Windows 11 x64.
- **[Android app](https://github.com/Teninz/obs-lan-camera/releases/latest/download/OBS-LAN-Camera-Android.apk)** — Android 8 or later.
- [All files and release notes](https://github.com/Teninz/obs-lan-camera/releases/latest) · [SHA-256](https://github.com/Teninz/obs-lan-camera/releases/latest/download/SHA256SUMS.txt).

Run the EXE and select the cards you need:

| Selection | Purpose |
| --- | --- |
| **Desktop app** | Up to five separate cameras for compatible calling apps and OBS. |
| **OBS plugin** | Connect a phone directly to an OBS scene. |
| **Both** | Five independent cameras through the shared receiver in OBS. |

Click **Install selected**. The desktop app requires **WebView2 Runtime**; the plugin requires **OBS Studio 33 x64**. Install the APK on your phone and allow camera access; microphone access is optional. **USB debugging is not required.**

## Connect over Wi-Fi

1. Connect your PC and phone to the same router. Ethernet on the PC + Wi-Fi on the phone work together.
2. On the PC: **MultiCamera T.E. → Connect** on a camera card → **Wi-Fi / LAN**.
3. On the phone: **QR → scan the right-hand connection code → round Start button**. The left-hand code downloads the APK.
4. Select **MultiCamera T.E. 1–5** in your calling app. Use a different card for each phone. Keep the desktop receiver running.

**In OBS:** add a **MultiCamera T.E. — Smartphone** source and select the camera number. Create a separate source for each camera.

**Plugin only:** add **Smartphone camera over Wi-Fi**, open the connection window in its source properties and scan its QR code with the phone app.

## USB and Internet

- **USB:** connect a data cable → enable **USB tethering** in Android → select **USB cable** and the phone's adapter on the PC → **Allow USB connection** → scan the QR code and start streaming.
- **Internet:** requires your own WSS relay with a trusted HTTPS certificate. Enter it in the desktop settings. [Server setup](relay/README.md) (Russian).

## Good to know

- Start with **720p / 30 FPS**. 60 FPS and stabilization depend on the camera and mode. Stabilization priority selects a mode up to 1080p/30.
- **Screen off:** tap the moon → **Stream with screen off** → allow notifications → turn off the screen with the power button. Manage streaming from the notification. High-speed 60 FPS currently switches to 30 FPS in the background.
- Windows cameras output **720p**. Phone audio is available in OBS; a separate Windows virtual microphone is not included yet. Multiple simultaneous cameras depend on the receiving app.
- Connection trouble? Check the shared router, guest Wi-Fi isolation and local firewall permissions.

**Experimental release.** [Tests and limitations](VERIFICATION.md) (Russian) · [Updates and removal](docs/MAINTENANCE.md) (Russian) · [Build instructions](docs/BUILD.md) · [GPL-2.0-or-later](COPYING).
