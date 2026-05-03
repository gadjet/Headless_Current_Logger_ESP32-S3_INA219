
# ESP32 Headless Solar Current Logger

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Hardware: Seeed Studio Xiao ESP32 S3](https://img.shields.io/badge/Hardware-Xiao%20ESP32%20S3-blue)](https://www.seeedstudio.com/XIAO-ESP32S3-p-5627.html)

A headless, web-configurable current logger designed to monitor solar panel charging in 12V vehicle systems. This device is designed to replace a standard fuse in a vehicle's fuse box, providing real-time and historical data on energy production and consumption.
It can also be used to monitor any low voltage DC current upto 3 Amps. 

| Top | Bottom | 
|-----------|--------------|
| <img width="216" height="120" alt="3D_PCB1_2026-05-03" src="https://github.com/user-attachments/assets/a752cda5-615b-4cd4-8f89-23f12833fb76" />| <img width="216" height="120" alt="3D_PCB1a_2026-05-03" src="https://github.com/user-attachments/assets/747fa24b-4832-4d43-b8d5-ecabebdd28be" />|


## 🚀 Key Features

- **Headless Operation:** No physical buttons or screens. Full control via a mobile-friendly web dashboard.
- **WiFi Access Point:** Broadcasts its own network (**"SolarLogger"**) for easy connection in the field.
- **Advanced Web Dashboard:**
  - **Real-time Monitoring:** Live current (mA) and bus voltage (V) tracking.
  - **Integrated Session Stats:** Track Peak Charge, Peak Draw, and total Session Energy (mAh) at a glance.
  - **Live Bidirectional Chart:** Visualizes charging (Green) vs. discharging (Red) with dynamic Y-axis scaling.
  - **Historical Log Viewer:** View stored `.csv` files as interactive charts directly in the browser.
  - **Remote File Manager:** List, download, and delete log files over WiFi.
  - **Auto Time-Sync:** Automatically synchronizes with the connected smartphone's time and local timezone upon connection.
- **Configurable Logging:** Adjustable intervals (1s to 1min) and automatic file rotation.
- **CSV Data Logging:** Standard format compatible with Excel and data analysis tools.

## 🛠 Hardware Architecture

### Components
- **Microcontroller:** Seeed Studio Xiao ESP32 S3
- **Sensor:** INA219 (High-side current monitor, 100mΩ shunt)
- **Storage:** MicroSD Card Module (SPI interface)
- **Power:** Regulated vehicle power or USB-C (5V)

### Pin Mappings (Xiao ESP32 S3)
The firmware utilizes manual GPIO mapping to ensure consistent behavior across different board packages.

| Component | Pin Function | Xiao Pin | GPIO |
|-----------|--------------|----------|------|
| **INA219**| SDA          | D4       | 5    |
|           | SCL          | D5       | 6    |
| **SD Card**| CS          | D3       | 4    |
|           | MOSI         | D10      | 9    |
|           | MISO         | D9       | 8    |
|           | SCK          | D8       | 7    |

## 💻 Software & Installation

### Prerequisites
- **Arduino IDE 2.x**
- **ESP32 Board Manager:** Espressif Systems (Select **Seeed Studio XIAO ESP32S3**)
- **Required Libraries:**
  - `Adafruit_INA219` (by Adafruit)
  - `SD`, `WiFi`, `WebServer`, `SPI`, `Wire` (Built-in ESP32 libraries)

### Configuration
Key flags at the top of the `.ino` sketch:
- `ENABLE_VOLTAGE`: Set to `true` to include bus voltage in logs and the web dashboard.
- `DEBUG_SERIAL`: Set to `true` to enable Serial monitoring at 115200 baud.

### Installation
1. Clone this repository.
2. Open `Headless_Solar_Logger/Headless_Solar_Logger.ino` in Arduino IDE.
3. Install the required libraries via the Library Manager.
4. Select **Seeed Studio XIAO ESP32S3** as your board.
5. Upload the sketch to your device.

## 📖 Usage Instructions

1. **Connect:** Power the device and connect your smartphone or laptop to the **"SolarLogger"** WiFi network (no password by default).
2. **Access Dashboard:** Open a web browser and navigate to `http://192.168.4.1`.
3. **Sync Time:** The device will automatically sync its internal clock with your browser's time on the first visit.
4. **Start Logging:** Select your desired interval and click **"START LOGGING"**.
5. **Monitor:** Use the live chart to watch solar performance. Green indicates the battery is being charged; Red indicates a draw.
6. **Data Retrieval:** Scroll to the "Stored Logs" section to view charts of previous sessions or download the `.csv` files for deep analysis.

##  Webpage Layout

<img width="1440" height="6240" alt="WebPage" src="https://github.com/user-attachments/assets/facb4953-11d3-4beb-b582-676e353e536e" />



## ⚖️ License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details (or just enjoy the code!).
