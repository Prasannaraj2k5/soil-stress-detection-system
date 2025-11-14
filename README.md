🌱 Smart Plant Stress Detection System
ESP32 | IoT Dashboard | ThingSpeak Cloud | Stress Analytics | OTA | OLED UI

This project is a fully–featured Smart Irrigation & Plant Stress Monitoring System built using ESP32, DHT11, Soil Moisture Sensor, OLED Display, Relays, Buzzer, and ThingSpeak Cloud Integration.
It provides real-time plant stress detection, automatic irrigation, cloud monitoring, data export, and a beautiful web dashboard with analytics & charts.

🚀 Key Features
✅ 1. Real-Time Sensor Monitoring

Temperature (DHT11)

Humidity (DHT11)

Soil Moisture (Analog Sensor)

Stress Severity (0–10 scale)

Visual indicators & icons for stress levels

🌡 2. Advanced Stress Analysis Engine

Smart algorithm determines plant stress using:

Soil moisture drought levels

Temperature heat/cold stress

Humidity high/low stress

Produces: OPTIMAL, LOW, MODERATE, HIGH, CRITICAL

🔔 3. Smart Alert & Recommendation System

Automatic buzzer alert on HIGH or CRITICAL stress

Live recommendations displayed on the dashboard

Stress history storage

💧 4. Automatic & Manual Irrigation Control

Auto mode:

Turns ON pump when soil moisture ≤ threshold

Turns OFF pump when soil moisture becomes optimal

Manual mode control from the web dashboard

Relay status displayed in dashboard and OLED

☁️ 5. ThingSpeak Cloud Integration

Temperature

Humidity

Soil Moisture

Stress Level

Pump Status

Cloud Enable/Disable button in web UI

🖥 6. Professional Web Dashboard

Includes:

Stress dashboard

Interactive analytics chart

Export tab

Control center tab

Stress history

Beautiful UI (gradient, cards, tabs, animations)

📥 7. Data Logging + Export

Logs up to 50 sensor data points

Export options:

📊 JSON Export

📥 CSV Download

"Clear Data" option available

🖥️ 8. OLED Display Interface

OLED shows:

Stress level

Temperature

Humidity

Soil level

Pump status

Cloud icon

Stress bar indicator

🔄 9. OTA (Over-the-Air) Firmware Updates

Upload new firmware wirelessly.
Device name: plant-stress-detector

📡 Hardware Used
Component	Purpose
ESP32 DevKit V1	Main controller + WiFi
DHT11	Temperature & humidity sensor
Soil Moisture Sensor	Soil drought detection
0.96" SSD1306 OLED	Local display
Relay Module	Pump control
Buzzer	Stress alerts
Power Supply	5V/3.3V
🔌 Wiring Diagram (Summary)
ESP32 Pin	Component
D4	DHT11 Signal
34	Soil Moisture (Analog)
23	Relay IN
2	Buzzer
21	OLED SDA
22	OLED SCL
🌐 Web Dashboard Pages
📊 Stress Dashboard

Stress level visual bar

Soil, temp, humidity readings

Cloud sync status

Recommendations

Pump & mode status

Stress history

📈 Analytics

Line chart (Temp, Humidity, Soil, Stress)

Stress statistics

Live alerts

💾 Data Export

Download CSV

Download JSON

Clear Data

Preview last 5 logs

🎛 Control Center

Start/Stop pump

Auto/Manual mode

Enable/Disable ThingSpeak

System logs

🔧 Software Features

Non-blocking timers using millis()

OTA Update using ArduinoOTA

ThingSpeak HTTP GET API

WebServer with multiple endpoints

Adaptive stress scoring algorithm

Data logging with circular buffer

Interactive charts (Chart.js)

🧪 Endpoints Included
Endpoint	Purpose
/	Main dashboard
/control?command=on/off/auto/manual/cloud_on/cloud_off	Control system
/download-csv	CSV export
/export-json	JSON export
/clear-data	Reset logs
/thingspeak-enable	Enable cloud
/thingspeak-disable	Disable cloud
🛠 Installation
1️⃣ Install Required Libraries

Install in Arduino IDE:

WiFi.h
WebServer.h
Adafruit_GFX
Adafruit_SSD1306
DHT Sensor Library
ArduinoOTA
HTTPClient

2️⃣ Flash the Code

Upload using USB initially.
Then use OTA for wireless firmware updates.

3️⃣ Open Browser

Enter:

http://<your-esp32-ip-address>/


Dashboard will load automatically.

🧑‍💻 Project Name

Smart Plant Stress Detection and Automatic Irrigation with IoT Cloud & Analytics
Nanochip Mini Project | ECE Department

📷 Screenshots

<img width="1600" height="900" alt="image" src="https://github.com/user-attachments/assets/f6484fec-8aba-4f96-9671-93014046cda6" />
<img width="1600" height="900" alt="image" src="https://github.com/user-attachments/assets/ecc8ed33-9315-46d9-9a60-fba8590daa5e" />
<img width="1600" height="900" alt="image" src="https://github.com/user-attachments/assets/32818dfa-3d7a-4525-bb18-923d29446c97" />
<img width="1280" height="960" alt="image" src="https://github.com/user-attachments/assets/52de9e17-c768-4e72-8259-b6abc82e72f1" />


(Add your dashboard images here)

📜 License

This project is open-source and free to use for academic purposes.
