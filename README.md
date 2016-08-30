# UV furnace

## Table of contents

### 1. [Introduction](https://github.com/fablab-leoben/UV_furnace/blob/master/README.md#1-introduction-1 "Introduction")
### 2. [Safety first](https://github.com/fablab-leoben/UV_furnace/blob/master/README.md#2-safety-first-1 "Safety first")
### 3. [Bill of materials](https://github.com/fablab-leoben/UV_furnace/blob/master/README.md#2-bill-of-materials-1 "Bill of Materials")
### 4. [Required tools](https://github.com/fablab-leoben/UV_furnace/blob/master/README.md#3-required-tools-1 "Required tools")
### 5. [CAD design](https://github.com/fablab-leoben/UV_furnace/blob/master/README.md#4-cad-design-1 "CAD design")
### 6. [Circuit](https://github.com/fablab-leoben/UV_furnace/blob/master/README.md#5-circuit-1 "Circuit")
### 7. [Human Machine Interface (HMI)](https://github.com/fablab-leoben/UV_furnace/blob/master/README.md#6-human-machine-interface-hmi-1 "Human Machine Interface (HMI)")
#### 7.1 [HMI Screenshots](https://github.com/fablab-leoben/UV_furnace/blob/master/README.md#61-hmi-screenshots-1 "HMI Screenshots")
#### 7.2 [Flashing the display](https://github.com/fablab-leoben/UV_furnace/blob/master/README.md#62-flashing-the-display-1 "Flashing the display")
### 8. [UV furnace firmware](https://github.com/fablab-leoben/UV_furnace/blob/master/README.md#8-furnace-firmware-1 "UV furnace firmware")
#### 8.1 [Libraries]( "")
#### 8.2 [Firmware]( "")
### 9. [Smartphone app](https://github.com/fablab-leoben/UV_furnace/blob/master/README.md#6-app-1 "Smartphone app")
#### 9.1 [Screenshot](https://github.com/fablab-leoben/UV_furnace/blob/master/README.md#61-screenshot-1 "Screenshot")
### 10. [To Do](https://github.com/fablab-leoben/UV_furnace/blob/master/README.md#7-to-do-1 "To Do")

### 1. Introduction

This UV furnace is an open source oven for curing DLP Resin 3D prints.

### 2. Safety first

Be careful, this project includes light in the UV-A and UV-B spectrum. Please be aware this can harm your eyes and skin in both short and long term! So please take care of that when use build and use this furnace. I take no responsibility if you hurt yourself or others. All initial tests where done with LEDs in the visable light spectrum. After that special safety equipment was used if necessary. 
The furnace detects an open door through a reed switch and will shut down immediately to protect the user.
Please also take care that there is mains voltage involved which can kill you! If you are not sure what you do ask someone with enough experience. 
The heating pad can get up to 300 °C so make sure it cannot burn your house.
To protect the electronics from overheating in a case of malfunction the firmware checks continously the temperature of the DS3231 realtime clock and shuts everything of if necessary.

### 3. Bill of materials

| Pos. | Pieces | Unit |  Name                                                                     |          comment          |
|------|--------|------|---------------------------------------------------------------------------|---------------------------|
|   1  |   1    |  []  | Arduino Mega 2560 R3                                                      |                           |
|   2  |   1    |  []  | Arduino Ethernet Shield 2                                                 |                           |
|   3  |   1    |  []  | Adafruit Thermoelement-Verstärker MAX31855 breakout board - v2.0          |                           |
|   4  |   1    |  []  | Thermocouple Type-K Glass Braid Insulated - K                             |                           |
|   5  |   1    |  []  | Adafruit Magnetic contact switch (door sensor)                            | Normally open reed switch |
|   6  |   2    |  []  | Kingston 8 GB Micro-SDHC Class10 Speicherkarte                            | any Micro-SD card is ok   |
|   7  |   1    |  []  | Adafruit ChronoDot - Ultra-precise Real Time Clock - v2.1                 |                           |
|   8  |   1    |  []  | Adafruit VEML6070 UV Index Sensor Breakout                                | optional                  |
|   9  |   1    |  []  | Adafruit Rugged Metal Pushbutton with Red LED Ring                        |                           |
|  10  |   1    |  []  | Male DC Power adapter - 2.1mm plug to screw terminal block                |                           |
|  11  |   1    |  []  | 5V 1 channel relay shield for Arduino                                     |                           |
|  12  |   1    |  []  | MEAN WELL RS-25-12                                                        |                           |
|  13  |   1    |  []  | MEAN WELL LDD-350HW                                                       |                           |
|  14  |   2    |  []  | MEAN WELL LDD-500HW                                                       |                           |
|  15  |   3    |  []  | FISCHER ELEKTRONIK SK57750SA                                              |                           |
|  16  |   1    |  []  | MEAN WELL PM-10-5                                                         |                           |
|  17  |   1    |  []  | 240 V 50 W heater mat                                                     |                           |
|  18  |   1    |  []  | Nichia SMD LED UV NCSU275 365 nm                                          | on a 10 x 10 mm pcb       |
|  19  |   1    |  []  | Nichia SMD LED UV NCSU275 400 nm                                          | on a 10 x 10 mm pcb       |
|  20  |   1    |  []  | NIKKISIO 300 nm LED from EQ Photonics GmbH                                | ~ 260 €/piece (optional)  |
|  21  |   1    |  []  | Arctic Silver Wärmeleitkleber (2x 3.5g)                                   |                           |
|  22  |   1    |  []  | fused quarz glass plate 200 x 200 x 2 mm                                  |                           |
|  23  |   1    |  []  | Aluminium plate 200x200x3                                                 |                           |
|  24  |   1    |  []  | Geocel Plumba Flue Silikondichtmasse, bis+315°C, Kartusche, 310ml         | optional                  |
|  25  |   1    |  []  | IEC-Steckverbinder C14, Tafelmontage, gerade 10A 250 V ac                 |                           |
|  26  |  24    |  []  | Openbeam Corner Cubes                                                     |                           |
|  27  |  10    | [m]  | Openbeam beam                                                             |                           |
|  28  |  36    |  []  | Makerbeam Corner brackets                                                 |                           |
|  29  |   4    |  []  | Feet for Openbeam                                                         |                           |
|  30  |   1    |  []  | Thread forming screw set for OpenBeam                                     |                           |
|  31  |  24    |  []  | MakerBeam Right angle brackets                                            |                           |
|  32  |  12    |  []  | MakerBeam T brackets                                                      |                           |
|  33  |   2    |  []  | MakerBeam Button Head Socket Bolt                                         |                           |
|  34  |   1    |  []  | MakerBeam M3 regular nuts                                                 |                           |
|  35  |   1    |  []  | MakerBeam M3 knurled nuts                                                 |                           |
|  36  |   1    |  []  | MakerBeam M3 cap nuts                                                     |                           |
|  37  |   1    |  []  | USB 2.0 Anschlusskabel USB 2.0 Stecker A - USB 2.0 Stecker B 1.80 m       |                           |
|  38  |   1    |  []  | USB 2.0 Anschlusskabel USB 2.0 Stecker A - USB 2.0 Stecker B 0.50 m       |                           |
|  39  |   1    |  []  | USB-Einbaubuchsen vorne USB-Buchse Typ B · hinten USB-Buchse Typ A        |                           |
|  40  |   1    |  []  | RJ45-Einbaubuchse Buchse, Einbau, Stecker, gerade Pole: 8P8C RRJVA_RJ45   |                           |
|  41  |   1    |  []  | Netzkabel, C13 auf Stecker mit Schutzleiterkontakt, 2,5m, 16 A, 250 V     |                           |
|  42  |   5    |  []  | mirror flagstone 200 x 200 mm                                             | metallised with Aluminium |
|  43  |   1    |  []  | Nextion 4,3" Touch Display NX4827T043                                     |                           |
|  44  |   1    |  []  | [Arduino Mega Screw Shield](http://www.crossroadsfencing.com/BobuinoRev17/)| optional                 |
|  45  |   2    |  []  | Fan SUNON PF80321B1-000U-S99                                              |                           |
|  46  |   3    |  []  | Resistor 4,7 kOhm                                                         |                           |
|  47  |   1    |  []  | shrink tubing                                                             |                           |
|  48  |   1    |  []  | bell wire or other wires in different colors                              |                           |
|  49  |   2    |  []  | small hinge from your local DIY market                                    |                           |
|  50  |   1    | [m]  | three-pole electric cable 1,5 mm²                                         |                           |
|  51  |   5    | [m]  | eight-pole bell wire | or other wires with different colors |
|  52  |   1    |  []  | Female/Male 'Extension' Jumper Wires - 40 x 6"                            | to wire the sensors       |
|  53  |   1,5  | [m²] | plywood 12x1000x1500 mm                                                   |                           |

### 4. Required tools

| Pos. |            Tool                      |        comment          |
|------|--------------------------------------|-------------------------|
|  1   |  patience ;)                         |                         |
|  2   |  different Screwdrivers              |                         |
|  3   |  different hex key drivers           |                         |
|  4   |  3D Printer                          |                         |
|  5   |  soldering iron                      |                         |
|  6   |  drilling machine                    |                         |
|  7   |  different drills                    |                         |
|  8   |  hammer                              |                         |
|  9   |  small buzzsaw or Lasercutter        |                         |
| 10   |  multimeter                          |                         |
| 11   |  jigsaw                              |                         |
| 12   |  lighter                             | for shrink tubing       |
| 13   |  [Arduino IDE](https://www.arduino.cc/en/Main/Software)|                         |
| 14   |  [Nextion Editor](http://nextion.itead.cc/download.html)| to program the display |
| 15   |  Micro SD Card reader                |                         |


### 5. CAD design

The CAD Design was done with [Onshape](https://www.onshape.com/cad-pricing "Onshape"). It is free for Hobbyists, Makers.

You can find and download the UV furnace design [here](https://cad.onshape.com/documents/65acaf65361afb5a9c027038/w/3b13638207107c108fe9135d/e/915e14169c92d702a1d86d80 "UV furnace design").
Please feel free to use other materials for your furnace design and just take measurements as you need them. 

### 6. Circuit

I have designed the circuit with [Fritzing](http://fritzing.org/download/). Download the free software to be able to modify the [circuit](https://github.com/fablab-leoben/UV_furnace/blob/master/circuit/UV_furnace_circuit.fzz).

![alt text](https://github.com/fablab-leoben/UV_furnace/blob/master/circuit/UV_furnace_circuit.png "Circuit")

### 7. Human Machine Interface (HMI)

The HMI was designed with [Pencil](http://pencil.evolus.vn/), an open-source GUI prototyping tool that's available for ALL platforms. The interface is based on the [Material Design Guidelines](https://material.google.com/) from Google for better useability. You can design each page with the GUI software. For example: you can design one screen where all buttons of your page are off. Then you save the same screen a second time where all buttons are on and a third time where all buttons are pressed. Now you can import these pictures into the [Nextion Editor](http://nextion.itead.cc/download.html) and place button objects, etc on your buttons. The Nextion editor uses partial overlays of your images. This makes designing beautiful interfaces very easy. See the files LED Setup off.png, LED Setup on.png, LED Setup pressed.png for a better imagination. The display has its own processor so it does not use ressources of your Arduino.

#### 7.1 HMI Screenshots

The start screen is shown during the device boots while everything is initialized.

![alt-text](https://github.com/fablab-leoben/UV_furnace/blob/master/HMI/Start.png "Start screen")

After the start screen you will see the overview page. There you can turn the furnace on/off got to settings and see the timer, current temperature and temperature diagram.

![alt-text](https://github.com/fablab-leoben/UV_furnace/blob/master/HMI/Overview.png "Overview screen")

From the settings screen you can use preconfigured settings which are stored on the SD card which you put into the SD card slot of the ethernet shield. Please see [preset1.cfg](https://github.com/fablab-leoben/UV_furnace/blob/master/preset1.cfg) for an easy example.
This will save you time as soon as you have found the ideal settings for curing your 3D printed parts.

![alt-text](https://github.com/fablab-leoben/UV_furnace/blob/master/HMI/Settings%20off.png "Settings screen")

From the settings screen you can switch to the temperature setup, UV LED setup, timer setup and PID setup:

![alt-text](https://github.com/fablab-leoben/UV_furnace/blob/master/HMI/Temp%20Setup%20off.png "Temperature screen")
![alt-text](https://github.com/fablab-leoben/UV_furnace/blob/master/HMI/LED%20Setup%20on.png "LED screen")
![alt-text](https://github.com/fablab-leoben/UV_furnace/blob/master/HMI/Timer%20Setup.png "Timer screen")
![alt-text](https://github.com/fablab-leoben/UV_furnace/blob/master/HMI/PID.png "PID screen")
![alt-text](https://github.com/fablab-leoben/UV_furnace/blob/master/HMI/Credits.png "Credits screen")

Changing the PID values is currently not supported because my calculations are really good at the moment, but it will be implemented soon. There is also a PID autotuning feature integrated which will automatically determine decent tuning constants for the Arduino PID Library. This feature currently cannot be triggered by the user. 

If the UV furnace detects an error, it shuts down and switches to the error screen:

![alt-text](https://github.com/fablab-leoben/UV_furnace/blob/master/HMI/Error.png "Error screen")

#### 7.2 Flashing the display

Save the [UV_furnace.tft](https://github.com/fablab-leoben/UV_furnace/blob/master/HMI/UV_furnace.tft) file on a micro SD card, insert it into the display and supply it with power. The flash process starts automatically. After that remove the micro SD card and repower the display.
If you would like to make changes to the interface just open [UV_furnace.HMI](https://github.com/fablab-leoben/UV_furnace/blob/master/HMI/UV_furnace.HMI) with the [Nextion editor](http://nextion.itead.cc/download.html).

### 8. UV furnace firmware
### 8.1 Arduino Libraries

### 8.2 Firmware

### 9. Smartphone app

Download and install the Blynk app for your [Android](https://play.google.com/store/apps/details?id=cc.blynk) or [iOS](https://itunes.apple.com/at/app/blynk-iot-for-arduino-raspberry/id808760481?mt=8) device. Now scan the QR code below and replace authentication code in the access.h file. Currently you can see the the setpoint and current temperature. You will get a notification when the furnace is ready. More features will come in the future.

![alt text](https://github.com/fablab-leoben/UV_furnace/blob/master/miscellaneous/QR_code.png "UV furnace App")

#### 9.1 Screenshot

![alt text](https://github.com/fablab-leoben/UV_furnace/blob/master/miscellaneous/APP_Screenshot.jpg "Screenshot")

### 10. To Do
* preheat function
* PID finetuning
* PID tuning without reflashing the sketch
* improve App
