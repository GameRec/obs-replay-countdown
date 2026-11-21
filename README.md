# Replay Countdown for OBS Studio

[![Ko-fi](https://img.shields.io/badge/Support%20on-Ko--fi-FF5E5B?style=for-the-badge&logo=ko-fi&logoColor=white)](https://ko-fi.com/gamerec)

Lightweight OBS Studio plugin that displays an on-screen, capture-excluded countdown timer for the **Replay Buffer** and automates back-to-back clip chaining without losing footage.

Built for gameplay creators by **[GameRec](https://gamerec.org)**.

---

## Key Features

 **Auto-Chain Save (`Save + Countdown`)**: Immediately saves the current replay buffer, starts a countdown synced to your OBS Replay Buffer duration (minus a configurable safety offset), and automatically triggers a second save before old footage is overwritten.
 **Action Window (`Countdown Only`)**: Starts a visual countdown showing how much buffer time remains to save the beginning of an ongoing event. Automatically clears the overlay once you save the replay.
 **100% Hidden from Capture**: Uses Windows `WDA_EXCLUDEFROMCAPTURE` — visible on your screen, completely invisible in OBS recordings, streams, Display Capture, Discord, and screenshots.
 **Multi-Monitor & Custom Styling**: Choose target display, screen corner, font size (`pt`), and overall UI scale (`%`).
 **Native Localization**: Automatically adapts to English (`en-US`) and Polish (`pl-PL`) OBS Studio locales.

---

## Installation (Windows x64)

### Option A: Automated Installer (Recommended)
1. Download `obs-replay-countdown-1.0.0-windows-x64-Installer.exe` from the **[Releases](../../releases)** page.
2. Run the installer (it automatically detects your OBS Studio directory).
3. Restart OBS Studio.

### Option B: Portable ZIP
1. Download `obs-replay-countdown-1.0.0-windows-x64.zip` from **[Releases](../../releases)**.
2. Extract and copy the `obs-plugins` and `data` folders directly into your OBS Studio installation directory (default: `C:\Program Files\obs-studio\`).
3. Restart OBS Studio.

---

## Usage

1. In OBS Studio, open **Docks → Replay Countdown** (*Panele → Replay Countdown*).
2. Adjust your **Auto-Chain safety offset**, target **Display monitor**, **Overlay position**, and **Font size**.
3. Go to **Settings → Hotkeys**, search for `Replay Countdown`, and bind keys to:
   - `Replay Countdown: Auto-Chain Save (Save + Countdown)`
   - `Replay Countdown: Action Window (Countdown Only)`
4. Start the **Replay Buffer** in OBS and use your hotkeys in-game.

---

## Support the Project

If this plugin saves your clips and makes your workflow easier, consider buying me a coffee — it directly supports building more free tools for creators:

👉 **[Support GameRec on Ko-fi](https://ko-fi.com/gamerec)**

---

## License

GPL-2.0 — see [LICENSE](LICENSE) for details.