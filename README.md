# MaatFirmware
Firmware for Maat - an alarm clock that you have to stand on for several minutes to disarm

# Temporary User Guide

## Basic Operation:

1. The USB Port is for programming only, not for power (make sure it's plugged in to the wall with the provided 5V power supply)
2. Make sure it's turned on and the back
3. Wait for it to turn on and the screen to turn off before stepping on (the scale sets the zero level after initialization)
4. Step on it - the light should come on and show the time and your weight
5. Press the SET button to enter the menu

  - Press the UP and DOWN buttons to navigate the menus
  - On the Alarm Arm menu, press the SET button to toggle the alarm on or off
  - On the Set Alarm menu, press the SET button to begin setting the time - press the UP and DOWN buttons to change the alarm, press the SET button to apply, hold the SET button down for a while to discard your changes
  - On the Set Time menu, press the SET button to begin setting the time - press the UP and DOWN buttons to change the time, press the SET button to apply, hold the SET button down for a while to discard your changes
  - On the Set Disarm Time menu, press the SET button to begin setting the disarm time (how long you need to stand on the scale to disarm the alarm each morning) - press the UP and DOWN buttons to change the disarm time, press the SET button to apply, hold the SET button down for a while to discard your changes
  - On the Tare menu, get off the scale and press the SET button quickly to tare the scale (reset the zero point). Don't hold the button down as you're putting weight on the scale while pressing the button so you'll mess up the tare. This menu also shows the current exact reading for diagnostics.

## When the Alarm Goes Off:

1. The buzzer will buzz it's little heart out
2. Stand on the scale to stop the buzzer - the screen will display the disarm animation
3. After the Disarm Time has passed and you haven't gotten off the scale, it will upload your weight to the server and the alarm will turn off
4. If you get off the scale at any point during the disarm period, the timer will reset!

## Calibration:

1. Hold the SET button while turning the scale on to enter calibration mode
2. Make sure you know your exact weight before beginning - you'll need to use another (trusted) scale for this
3. Press the UP and DOWN buttons simultaneously to tare the scale
4. Press the UP and DOWN button to change the calibration factor - this will change the displayed weight while you stand on it.
5. Press the SET button to change the units (0.1, 1, 10, 100, 1000) to do coarser or finer calibration changes
6. Hold the SET button to apply the calibration for all future scale power-ons
7. Reset the scale after applying the calibration to use the new calibration factor
