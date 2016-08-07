# UV furnace

## Table of contents

### 1. [Introduction](https://github.com/fablab-leoben/UV_furnace/blob/develop/README.md#1-introduction-1 "Introduction")
### 2. [Bill of materials](https://github.com/fablab-leoben/UV_furnace/blob/develop/README.md#2-bill-of-materials-1 "Bill of Materials")
### 3. [CAD design](https://github.com/fablab-leoben/UV_furnace/blob/develop/README.md#3-cad-design-1 "CAD design")

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
|  20  |   1    |  []  | NIKKISIO 300 nm LED from EQ Photonics GmbH                                | ~ 260 € per piece         |
|  21  |   1    |  []  | Arctic Silver Wärmeleitkleber (2x 3.5g)                                   |                           |
|  22  |   1    |  []  | fused quarz glass plate 200 x 200 x 2 mm                                  |                           |
|  23  |   1    |  []  | Aluminium plate 200x200x3                                                 |                           |
|  24  |   1    |  []  | Geocel Plumba Flue Silikondichtmasse, bis+315°C, Kartusche, 310ml         |                           |
|  25  |   1    |  []  | IEC-Steckverbinder C13, Tafelmontage, Anschluss Löten gerade 10A 250 V ac |                           |
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
|  41  |   5    |  []  | mirror flagstone 200 x 200 mm                                             | metallised with Aluminium |
|  42  |   1    |  []  | Nextion 4,3" Touch Display NX4827T043                                     |                           |
|  43  |   1    |  []  | [Arduino Mega Screw Shield](http://www.crossroadsfencing.com/BobuinoRev17/)| optional                 |
|  44  |   2    |  []  | Fan SUNON PF80321B1-000U-S99                                              |                           |
|  45  |   3    |  []  | Resistor 4,7 kOhm                                                         |                           |

### 3. CAD design

The CAD Design was done with [Onshape](https://www.onshape.com/cad-pricing "Onshape"). It is free for Hobbyists, Makers.

You can find and download the UV furnace design [here](https://cad.onshape.com/documents/65acaf65361afb5a9c027038/w/3b13638207107c108fe9135d/e/915e14169c92d702a1d86d80 "UV furnace design").

### 4. Circuit

![alt text](https://github.com/fablab-leoben/UV_furnace/blob/develop/circuit/UV_furnace_circuit.png "Circuit")
