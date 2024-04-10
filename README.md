This is a quick and dirty implementation of a Display and MQTT Client for Lelit MaraX V2 Espresso Machines.

Despite many other similar projects, this one is using the correct wiring and coding!

Please also visit https://www.m1n1.de/en/lelit-mara-x-v2-gicar-internals/ for details.

You need to add your WLAN SSID and Password to the secrets.h file, for getting acces to your WLAN. The Machine will appear in your router as MaraX. The IP provided by DHCP is also published on the serial monitor during strtup.
