# UV furnace

## Table of contents

### 1. [Introduction](https://github.com/fablab-leoben/UV_furnace/blob/master/README.md#1-introduction-1 "Introduction")
### 2. [Bill of materials](https://github.com/fablab-leoben/UV_furnace/blob/master/README.md#2-bill-of-materials-1 "Bill of Materials")
### 3. [Required tools](https://github.com/fablab-leoben/UV_furnace/blob/master/README.md#3-required-tools-1 "Required tools")
### 4. [CAD design](https://github.com/fablab-leoben/UV_furnace/blob/master/README.md#4-cad-design-1 "CAD design")
### 5. [Circuit](https://github.com/fablab-leoben/UV_furnace/blob/master/README.md#5-circuit-1 "Circuit")
### 6. [Human Machine Interface (HMI)](https://github.com/fablab-leoben/UV_furnace/blob/master/README.md#6-human-machine-interface-hmi-1 "Human Machine Interface (HMI)")
#### 6.1 [HMI Screenshots](https://github.com/fablab-leoben/UV_furnace/blob/master/README.md#61-hmi-screenshots-1 "HMI Screenshots")
#### 6.2 [Flashing the display](https://github.com/fablab-leoben/UV_furnace/blob/master/README.md#62-flashing-the-display-1 "Flashing the display")
### 7. [Arduino MEGA 2560](https://github.com/fablab-leoben/UV_furnace/blob/master/README.md#7-arduino-mega-2560-1 "Arduino MEGA 2560")
#### 7.1 [Arduino IDE](https://github.com/fablab-leoben/UV_furnace/blob/master/README.md#71-arduino-ide-1 "Arduino IDE")
#### 7.2 [Installing the libraries](https://github.com/fablab-leoben/UV_furnace/blob/master/README.md#72-installing-the-libraries-1 "Installing the libraries")
#### 7.3 [Flashing the code](https://github.com/fablab-leoben/UV_furnace/blob/master/README.md#73-flashing-the-code-1 "Flashing the code")
### 8. [App](https://github.com/fablab-leoben/UV_furnace/blob/master/README.md#6-app-1 "App")
#### 8.1 [Screenshot](https://github.com/fablab-leoben/UV_furnace/blob/master/README.md#61-screenshot-1 "Screenshot")
#### 8.2 [Measuring the temperature](https://github.com/fablab-leoben/UV_furnace/blob/master/README.md#82-measuring-the-temperature-1 "Measuring the temperature")
#### 8.3 [Time and alarms](https://github.com/fablab-leoben/UV_furnace/blob/master/README.md#83-time-and-alarms-1 "Time and alarms")
#### 8.4 [Relay](https://github.com/fablab-leoben/UV_furnace/blob/master/README.md#84-relay-1 "Relay")
#### 8.5 [LEDs](https://github.com/fablab-leoben/UV_furnace/blob/master/README.md#85-leds-1 "LEDs")
#### 8.6 [Persistant data](https://github.com/fablab-leoben/UV_furnace/blob/master/README.md#86-persistant-data-1 "Persistant data")
### 9. [Logging and visualization](https://github.com/fablab-leoben/UV_furnace/blob/master/README.md#9-logging-and-visualization-1 "Logging and visualization")
#### 9.1 [Requirements](https://github.com/fablab-leoben/UV_furnace/blob/master/README.md#91-requirements-1 "Requirements")
#### 9.2 [Install InfluxDB & Grafana](https://github.com/fablab-leoben/UV_furnace/blob/master/README.md#92-install-influxdb--grafana-1 "Install InfluxDB & Grafana")
#### 9.3 [Configuration](https://github.com/fablab-leoben/UV_furnace/blob/master/README.md#93-configuration-1 "Configuration")
##### 9.3.1 [InfluxDB](https://github.com/fablab-leoben/UV_furnace/blob/master/README.md#931-influxdb "InfluxDB")
##### 9.3.2 [Grafana](https://github.com/fablab-leoben/UV_furnace/blob/master/README.md#932-grafana "Grafana")
#### 9.4 [Demonstration](https://github.com/fablab-leoben/UV_furnace/blob/master/README.md#94-demonstration-1 "Demonstration")
### 10. [To Do](https://github.com/fablab-leoben/UV_furnace/blob/master/README.md#7-to-do-1 "To Do")

### 1. Introduction

3D printing is an advanced and environmentally friendly manufacturing method. It is a rapidly growing market both private and commercial. It is not just that they help you to act out your creativity or building prototypes, you are also able to repair things where you will not get any spare parts. They are also used in the medical and aerospace industry!
[SLA](https://en.wikipedia.org/wiki/Selective_laser_sintering) and [DLP](https://en.wikipedia.org/wiki/Digital_Light_Processing) printer are growing increasingly from an expensive niche to a consumer product.
The [Fablab in Leoben](http://www.fablab-leoben.at/) has a commercial [Form One](http://formlabs.com/de/produkte/3d-printers/form-1-plus/) and a **self built** [DLP printer](https://github.com/fablab-leoben/Fab-dlp). These printers usually need some form of post curing with additional UV light and heat to strengthen the printed parts. This would help to open more application areas for SLA/DLP resin printer to help saving ressources and our environment.
During our DLP printer build and because of our experience with the Form One we tried several methods to post-cure the parts after printing. The perfect solution was not found in our experiments so we decided to build our own UV furnace.

Broken parts printed with a Formlabs Form One SLA printer:
![alt-text](https://github.com/fablab-leoben/UV_furnace/blob/master/miscellaneous/broken_parts.jpg)

DLP Printer of the Fablab Leoben:
![alt-text](https://github.com/fablab-leoben/UV_furnace/blob/master/miscellaneous/Fablab_Leoben_DLP.jpg)

Successfull print of [Aria the Dragon](https://www.youmagine.com/designs/aria-the-dragon)
![alt-text](https://github.com/fablab-leoben/UV_furnace/blob/master/miscellaneous/Aria_the_dragon.jpg)
It looks great but you have to be very careful because the resin is not fully cured and an ideal candidate for the UV furnace.

During the last few months I have designed and built this furnace. It is already working but still under heavy development to include new features and fix small bugs. 

### 2. Bill of materials

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
|   9  |   1    |  []  | Adafruit Rugged Metal Pushbutton with LED Ring                            |                           |
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
|  39  |   1    |  []  | USB-Einbaubuchse vorne USB-Buchse Typ B · hinten USB-Buchse Typ A         | optional                  |
|  40  |   1    |  []  | RJ45-Einbaubuchse Buchse, Einbau, Stecker, gerade Pole: 8P8C RRJVA_RJ45   | optional                  |
|  41  |   1    |  []  | Netzkabel, C13 auf Stecker mit Schutzleiterkontakt, 2,5m, 16 A, 250 V     |                           |
|  42  |   5    |  []  | mirror flagstone 200 x 200 mm                                             | metallised with Aluminium |
|  43  |   1    |  []  | Nextion 4,3" Touch Display NX4827T043                                     |                           |
|  44  |   1    |  []  | [Arduino Mega Screw Shield](http://www.crossroadsfencing.com/BobuinoRev17/)| optional                 |
|  45  |   2    |  []  | Fan SUNON PF80321B1-000U-S99                                              | optional                  |
|  46  |   3    |  []  | Resistor 4,7 kOhm                                                         |                           |
|  47  |   1    |  []  | shrink tubing                                                             |                           |
|  48  |   1    |  []  | bell wire or other wires in different colors                              |                           |
|  49  |   2    |  []  | small hinge from your local DIY market                                    |                           |
|  50  |   1    | [m]  | three-pole electric cable 1,5 mm²                                         |                           |
|  51  |   5    | [m]  | eight-pole bell wire | or other wires with different colors |
|  52  |   1    |  []  | Female/Male 'Extension' Jumper Wires - 40 x 6"                            | to wire the sensors       |
|  53  |   1,5  | [m²] | plywood 12x1000x1500 mm                                                   |                           |

Estimated costs: **~1000 €**

It really depends on your manufacturing possibilities and the shops where you buy the components. Also be aware that we are using a 300 nm UV LED for **~260 €/piece**! Try to recycle parts when you have a low budget and drop expensive nice to have parts like the "USB-Einbaubuchse" and "RJ45-Einbaubuchse" which cost ~40 € each. So there is enough room to cut the costs down :)

### 3. Required tools

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


### 4. CAD design

![alt-text](https://github.com/fablab-leoben/UV_furnace/blob/master/miscellaneous/UV_furnace_CAD_design.JPG "UV furnace CAD design")

The CAD Design was done with [Onshape](https://www.onshape.com/cad-pricing "Onshape"). It is free for Hobbyists, Makers.

You can find, modify and download the UV furnace design [here](https://cad.onshape.com/documents/65acaf65361afb5a9c027038/w/3b13638207107c108fe9135d/e/915e14169c92d702a1d86d80 "UV furnace design"). Feel free to modify the housing to fit your budget. 

### 5. Circuit

I have designed the circuit with [Fritzing](http://fritzing.org/download/). Download the free software to be able to modify the [circuit](https://github.com/fablab-leoben/UV_furnace/blob/master/circuit/UV_furnace_circuit.fzz).
Wire everything like on the picture below and check everything at least twice to make sure everything is wired correctly! Otherwise you can destroy your hardware and jeopardise your health.

![alt text](https://github.com/fablab-leoben/UV_furnace/blob/master/circuit/UV_furnace_circuit.png "Circuit")

Everything mounted in the furnace:
![alt-text](https://github.com/fablab-leoben/UV_furnace/blob/master/miscellaneous/Electronics.jpg "Electronics")

### 6. Human Machine Interface (HMI)

The HMI was designed with [Pencil](http://pencil.evolus.vn/), an open-source GUI prototyping tool that's available for ALL platforms. The interface is based on the [Material Design Guidelines](https://material.google.com/) from Google for better useability.

#### 6.1 HMI Screenshots

#### 6.2 Flashing the display

Save the [UV_furnace.tft](https://github.com/fablab-leoben/UV_furnace/blob/master/HMI/UV_furnace.tft) file on a micro SD card, insert it into the display and supply it with power. The flash process starts automatically. After that remove the micro SD card and repower the display.
If you would like to make changes to the interface just open [UV_furnace.HMI](https://github.com/fablab-leoben/UV_furnace/blob/master/HMI/UV_furnace.HMI) with the [Nextion editor](http://nextion.itead.cc/download.html).

### 7. Arduino Mega 2560

Because of the comprehensive features I had to use an Arduino Mega for this project.
Visit the [Getting started page](https://www.arduino.cc/en/Guide/HomePage) for more information on how to configure your board.

| Technical specification |              |
|-------------------------|--------------|
| Microcontroller   |	ATmega2560 |
| Operating Voltage |	5V |
| Input Voltage (recommended) |	7-12V |
| Input Voltage (limit) |	6-20V |
| Digital I/O Pins |	54 (of which 15 provide PWM output) |
| Analog Input Pins |	16 |
| DC Current per I/O Pin |	20 mA |
| DC Current for 3.3V Pin |	50 mA |
| Flash Memory |	256 KB of which 8 KB used by bootloader |
| SRAM	| 8 KB |
| EEPROM	| 4 KB |
| Clock Speed	| 16 MHz |
| Length |	101.52 mm |
| Width |	53.3 mm |
| Weight |	37 g |

#### 7.1 Arduino IDE

First of all download the [Arduino IDE](https://www.arduino.cc/en/Main/Software) and install it on your computer. It is available for Windows, Linux and Mac.

#### 7.2 Installing the libraries

Download the libraries and extract them into your Arduino libraries folder. The path for Windows computer is: 
>C:\Users\YourName\Documents\Arduino\libraries
If you do not have this folder do not hesitate to create it.

#### 7.3 Flashing the code

Open the Arduino IDE and connect your Arduino via USB to your computer. Under Tools/Board select Arduino Mega and choose the right serial port. 
Open the sketch UV_furnace.ino and click on the compile & flash button. The compiler now translates the code and writes it to your microcontroller.  

### 8. Noteworthy things


#### 8.1. Finite State Machine

My code uses a finite state machine (FSM) to make reading and understanding such a massive amount of code easier for everyone.

Definition of a Finite State Machine:
>A finite-state machine (FSM) or finite-state automaton (FSA, plural: automata), or simply a state machine, is a mathematical model of >computation used to design both computer programs and sequential logic circuits. It is conceived as an abstract machine that can be >in one of a finite number of states. The machine is in only one state at a time; the state it is in at any given time is called the >current state. It can change from one state to another when initiated by a triggering event or condition; this is called a >transition. A particular FSM is defined by a list of its states, and the triggering condition for each transition.

The FSM serves as a manager that organizes the states of the UV furnace.

#### 8.2 Measuring the temperature

The furnace uses a MAX31855 thermocouple amplifier to measure the temperature. You can use thermocouples also for temperatures over 1000 °C. Only the thermocouple is in the hot part of the furnace so you do not need to worry about damaging the amplifier.

#### 8.3 Time and alarms

During startup the furnace requests the current time via the Network Time Protocol (NTP) to set the time of the DS3231 realtime clock. The realtime clock is used to generate an alarm when the furnace has finished. The integrated temperature compensation is also used to check the temperature of the electronics and acts as a safety feature.

#### 8.4 Relay

The UV furnace has a high thermal mass, so the response time is slow. Therefore it is possible to use a cheap relay instead of a solid state relay to control the heating pad. In our case the response time will be 0,1 Hz or once every 10 seconds.
Take care of yourself when you wire the relay because there is mains voltage involved!

#### 8.5 LEDs

I use Meanwell LDD-500H LED driver to control the LEDs. They offer a wide input voltage range from 9 ~ 56 VDC and a high output voltage range from 2 ~ 52 VDC. Hence you are very flexible when you would like to use other LEDs with a different wavelength in your furnace. You only need to change the power supply if necessary. At the moment I am using three LEDs (red, green, white) in the visible light spectrum. Due to the fact that the furnace is under heavy development it is much more comfortable to add features, fix small bugs and test the workflow.
This shows the flexibility of the LED setup. Switching to UV LEDs is only a matter of minutes.

To be able to cure the 3D printed parts the UV light needs to light the curing area. We do not want to stress our light source with unnecessary heat when we cure our prints. Regular glass blocks most of the UV radiation. Because of that we use a quartz glass plate below the LEDs.

![alt-text](https://github.com/fablab-leoben/UV_furnace/blob/master/miscellaneous/mounted_LEDs_with_heatsinks.jpg)

#### 8.6 Persistant data

The tuning parameters for the PID controller are saved in the Arduino's EEPROM. Thus the UV furnace will remember the settings from the last time. EEPROM can only be written a finite number of times so we compare the contents before writing.

### 9. Logging and visualization

There are many use cases where data logging and visualization can be very useful so I have made an easy setup to achieve this functionality.

#### 9.1 Requirements
  * Computer running Ubuntu 
  * Install [Docker](docker.com)
  * create directory /media/nas/data/influxdb/
  * create directory /media/nas/data/grafana/
  
#### 9.2 Install InfluxDB & Grafana

Install command for InfluxDB
>sudo docker run -d -p 8083:8083 -p 8086:8086 --name influxdb -p 4444:4444/udp --expose 4444 -e UDP_DB="my_db" -v /media/nas/data/influxdb:/data --restart=unless-stopped tutum/influxdb:0.13

Install command for Grafana
>docker run -d -v /media/nas/data/grafana --name grafana-storage busybox:latest
>docker run -d -p 3000:3000 --name=grafana --volumes-from grafana-storage --restart=unless-stopped grafana/grafana:3.1.1

#### 9.3 Configuration
##### 9.3.1 InfluxDB
In your browser got to yourserver:8083
Change Database to my_db
Under Query Templates create a new user and password for the next step

![alt-text](https://github.com/fablab-leoben/UV_furnace/blob/master/miscellaneous/InfluxDB_config.JPG)

##### 9.3.2 Grafana

In your browser got to yourserver:3000 and add a new data source:
Name: Influxdb UDP
Type:InfluxDB
URL: yourserveraddress:8086
Access: Proxy
Database: my_db
User and Password you have created in the step before
Save and Test

![alt-text](https://github.com/fablab-leoben/UV_furnace/blob/master/miscellaneous/Grafana_data_source.JPG)

Create a new dashboard:
![alt-text](https://github.com/fablab-leoben/UV_furnace/blob/master/miscellaneous/Grafana_new_dashboard.JPG)

Add a new query:
![alt-text](https://github.com/fablab-leoben/UV_furnace/blob/master/miscellaneous/Grafana_query.JPG)

Save your dashboard and configure the time range in the right corner:
![alt-text](https://github.com/fablab-leoben/UV_furnace/blob/master/miscellaneous/Grafana_time_range.JPG)

#### 9.4 Demonstration

Start your furnace and you will see the incoming data in your graph:
![alt-text](https://github.com/fablab-leoben/UV_furnace/blob/master/miscellaneous/Grafana_demonstration.JPG)

### 10. App

The smartphone app makes it possible to monitor your furnace when you are not nearby. You will also get a notification when it has finished or if there is an error.

![alt text](https://github.com/fablab-leoben/UV_furnace/blob/master/miscellaneous/APP_Screenshot.jpg "Screenshot")

#### 10.1 App installation

Download and install the Blynk app for your [Android](https://play.google.com/store/apps/details?id=cc.blynk) or [iOS](https://itunes.apple.com/at/app/blynk-iot-for-arduino-raspberry/id808760481?mt=8) device. Now scan the QR code below and replace Auth code in the access.h file. Currently you can see the the setpoint and current temperature. You will get a notification when the furnace is ready. More features will be added in the future.

![alt text](https://github.com/fablab-leoben/UV_furnace/blob/master/miscellaneous/QR_code.png "UV furnace App")

### 11. To Do
* screensaver
* better display bezel
* preheat function
* PID finetuning
* PID tuning without reflashing the sketch
* improving the app
