This is a quick and dirty implementation of a Display and MQTT Client for Lelit MaraX V2 Espresso Machines.

Despite many other similar projects, this one is using the correct wiring and coding!
Please TAKE CARE, as most information in the Web regarding how to interface with MaraX Gicar Control Box is WRONG! 

Please also visit https://www.m1n1.de/en/lelit-mara-x-v2-gicar-internals/ for details.

You need to add your WLAN SSID and Password to the secrets.h file, for getting acces to your WLAN. The Machine will appear in your router as "MaraX".
Add IP Address and port of your MQTT Broker and desired update interval in seconds.  

Hardware used is an ESP8266 on a Wemos D1 clone and 128x64 Adafruit SSD1306 SPI OLED Display.

![image](https://github.com/dougie996/M1N1MaraX_MQTT/assets/117717919/8c066df9-6e21-4d42-b458-7699bd4b0714)



![MaraX_MQTT](https://github.com/dougie996/M1N1MaraX_MQTT/assets/117717919/a794e90c-03ee-4b7a-b04b-ac75f7c3cfe0)
