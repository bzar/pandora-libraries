#!/bin/bash
cur=$(cat /sys/devices/platform/twl4030-pwm0-bl/backlight/twl4030-pwm0-bl/brightness);
echo $(($cur+5)) > /sys/devices/platform/twl4030-pwm0-bl/backlight/twl4030-pwm0-bl/brightness
