# Perry3000 Smart Cooker

This firmware project was created using [Particle Developer Tools](https://www.particle.io/developer-tools/) and is compatible with all [Particle Devices](https://www.particle.io/devices/).


## Table of Contents
- [Introduction](#introduction)
- [Prerequisites To Use This Template](#prerequisites-to-use-this-repository)
- [Getting Started](#getting-started) 
- [Auto Cad Design](#Auto-Cad-Design) 
- [Schematic](#Schematic)
- [Fritzing Diagram](#Fitzing-Diagram)
- [About Me](#About-Me)
  - [Background](#Background)
  - [Interests](#Interests)
- [Version](#version)

## Introduction

The Smart Cooker program was conceived with the aim of transforming any conventional oven into an intelligent cooking appliance. Its primary objective is to alleviate the challenges faced by individuals in their daily meal preparation routines by automating the cooking process.

Upon activation by a simple button push, the program initiates its functionalities. It prompts users to scan an NFC recipe card at the appropriate moment or accepts recipe commands from an Adafruit dashboard. Once a recipe is identified, the oven preheats to the required temperature and prompts the user to place the ingredients inside.

Subsequently, the program oversees the cooking process, continuously monitoring the oven temperature. In the event of overheating, the system intervenes by temporarily shutting down the oven until it returns to the designated cooking temperature. Upon completion of cooking, the system ensures that the food cools to a safe temperature for handling, preventing any potential burns, before guiding the user to remove the cooked meal from the oven.

After the food is retrieved, the system transitions into an ultra-low power mode, conserving energy until reactivated by the user for future use.

This innovative system aims to streamline cooking tasks while prioritizing user safety and convenience.

## Prerequisites To Use This Repository

To use this software you'll need:

* Particle Photon 2
* DC 1 channel optocoupler 3V/3.3V relay for controlling oven power
* Adafruit Neopixel ring for LED status indication
* Adafruit monochrome OLED graphic display for visual feedback
* DFRobot Mini-MP3 player and speaker for verbal prompts
* DFRobot NFC communication controller
* NFC cards and/or tags for recipe input
* KY-003 Hall Effect Sensor Module
* 3 tactile buttons for on/off and volumn up/down

## Getting Started

1. Build the circuit.

2. While not essential, it is recommended running the [device setup process](https://setup.particle.io/) on your Particle device first. This ensures your device's firmware is up-to-date and you have a solid baseline to start from.

3. If you haven't already, open this project in Visual Studio Code (File -> Open Folder). Then [compile and flash](https://docs.particle.io/getting-started/developer-tools/workbench/#cloud-build-and-flash) your device. Ensure your device's USB port is connected to your computer.

4. Verify the device's operation by monitoring its logging output:
    - In Visual Studio Code with the Particle Plugin, open the [command palette](https://docs.particle.io/getting-started/developer-tools/workbench/#particle-commands) and choose "Particle: Serial Monitor".
    - Or, using the Particle CLI, execute:
    ```
    particle serial monitor --follow
    ```


## Auto Cad Design
3D model of the flower pot

<img src="HousePlantWaterSys/images/flower_pot.gif" width=500 height=350>

## Schematic
<img src="HousePlantWaterSys/images/schematic.png" width=500 height=450>

## Fitzing Diagram
<img src="HousePlantWaterSys/images/fritzing.png" width=500 height=450>

## About Me

I'm Kathryn Perry, a software developer with a passion for embedded programing and IoT projects. I specialize in leveraging technology to create innovative solutions that make a difference in people's lives. Check out my Hackster [https://www.hackster.io/bytecodeperry](https://www.hackster.io/bytecodeperry)

### Background
With a background in software development, I've honed my skills in creating efficient and effective software solutions. My journey in the tech world has led me to explore the fascinating realm of Internet of Things (IoT) projects, where I thrive on combining my programming expertise with my love for problem-solving.

### Interests
Beyond the digital realm, I have a deep-rooted interest in plants and gardening. There's something magical about nurturing greenery and watching it flourish, which is why I'm particularly drawn to projects that integrate technology with plant care.

## Version

Template version 1.0.0
