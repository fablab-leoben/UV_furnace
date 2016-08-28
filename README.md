# UV furnace

## Table of contents

### 1. [Introduction](https://github.com/fablab-leoben/UV_furnace/blob/master/README.md#1-introduction-1 "Introduction")
### 2. [Bill of materials](https://github.com/fablab-leoben/UV_furnace/blob/master/README.md#2-bill-of-materials-1 "Bill of Materials")
### 3. [Required tools](https://github.com/fablab-leoben/UV_furnace/blob/master/README.md#3-required-tools-1 "Required tools")
### 4. [CAD design](https://github.com/fablab-leoben/UV_furnace/blob/master/README.md#4-cad-design-1 "CAD design")
### 5. [Circuit](https://github.com/fablab-leoben/UV_furnace/blob/master/README.md#5-circuit-1 "Circuit")
### 6. [To Do](https://github.com/fablab-leoben/UV_furnace/blob/master/README.md#6-to-do-1 "To Do")

### 1. Introduction

This UV furnace is an open source oven for curing DLP Resin 3D prints.

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


### 4. CAD design

The CAD Design was done with [Onshape](https://www.onshape.com/cad-pricing "Onshape"). It is free for Hobbyists, Makers.

You can find and download the UV furnace design [here](https://cad.onshape.com/documents/65acaf65361afb5a9c027038/w/3b13638207107c108fe9135d/e/915e14169c92d702a1d86d80 "UV furnace design").

### 5. Circuit

![alt text](https://github.com/fablab-leoben/UV_furnace/blob/master/circuit/UV_furnace_circuit.png "Circuit")

### 6. To Do
