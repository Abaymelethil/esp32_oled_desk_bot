# ESP32 OLED Desk Bot 

An expressive ESP32-based desktop companion with animated emotions, touch interaction, clock, and weather display.

Inspired by cute desk robots and designed to be simple, hackable, and fun.

---

##  Features

- Animated facial expressions (Alive, Love, Angry, Sad, Dizzy)
- Touch-based interaction
  - Single tap
  - Double tap
  - Long press
- Clock mode (auto time via NTP)
- Weather mode (temperature + animated clouds)
- Sleep / wake behavior
- Smooth OLED animations

---

##  Hardware Required

- ESP32 Dev Board (Devkit V1 / ESP32-S3 recommended)
- 0.96\" OLED Display (128×64, SSD1306, I²C)
- Touch button / capacitive touch pad
- Jumper wires
- USB cable

---

##  Wiring

### OLED (SSD1306 – I²C)

| OLED | ESP32 |
|----|----|
| VCC | 3.3V |
| GND | GND |
| SDA | GPIO 21 |
| SCL | GPIO 22 |

### Touch Input

| Touch | ESP32 |
|-----|------|
| Signal | GPIO 27 |
| VCC (if needed) | 3.3V |
| GND | GND |

>  See wiring diagram images in `/docs`

---

##  Software Requirements

### Arduino IDE
- Arduino IDE 2.x recommended

### ESP32 Board Package
1. Open Arduino IDE
2. Go to **Preferences**
3. Add this to *Additional Board URLs*:
