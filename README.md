# Home Energy Monitor

## Problem Statement
There are many ways in which data on household electricity use could provide
value to homeowners. Yet, despite the widespread use of smart electricity
meters, this data is generally not available to consumers at a detailed level.
To address this problem, I'm building a monitoring device that will continuously
record the rate of electricity consumption in my home. The device will send its
data to the cloud in near real time, where it will be available to power
dashboards and enable future experimentation with machine learning algorithms.  

## Methodology
The monitoring device is powered by an Arduino Nano 33 IoT microcontroller
board. The device uses two split-core current transformers (sometimes referred
to as non-invasive current sensors) to measure current coming into the home.
Measurements are sent at regular intervals to an MQTT server running on the
AWS IoT Core platform. From there, a Lambda function processes each message
and files the data to a DynamoDB table. This table supplies data to dashboard
components that display electricity use in near real time.

## Resources
- [Xavier Decuyper's DIY Home Energy Monitor](https://savjee.be/2019/07/Home-Energy-Monitor-ESP32-CT-Sensor-Emonlib/)
- [OpenEnergyMonitor's Learning Site](https://learn.openenergymonitor.org/)
