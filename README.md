# Swisscom LPN Mapping device

<img src="img/proto1.jpg" alt="Prototype box" width="500" />

This device features is built on [Tuino 1](www.tuino.io) with [ST I-CUBE LRWAN](https://github.com/gimasi/GMX_LR1_AT_MODEM) and features the [Grove humidity and temperature sensor](http://wiki.seeed.cc/Grove-Temperature_and_Humidity_Sensor_Pro/) and a [Trimble GPS (Sparkfun Copernicus II DIP)](https://www.sparkfun.com/products/11858?_ga=2.245291909.129632425.1521044459-524054582.1519123891) on top of it. For the user interface, a [Grove OLED 0.96''](http://wiki.seeed.cc/Grove-OLED_Display_0.96inch/) and a [Grove button](http://wiki.seeed.cc/Grove-Button/) were added. 

## Hardware and connections
Connections
Box


## State machine and schematics

With the button, the user can switch between different tracking modes or suspend the tracking. This is particularily useful if you meet somebody on the campus and stop for a chat, then you don't want the same point to be mapped multiple times.

<img src="img/track_states.jpg" alt="Track state machine" width="300" />
The schematics of this first, outer state machine is shown above. As the rest of the code still uses the delay() function, the button has to be pressed for several seconds in order to take the change into account. 

###Track numbers
The track number is sent in the payload and stored together with the point in the server. It is later used to filter the points according to the experiment or store them in a different part of the database.
* Track 0: Mapping suspended (nothing sent)
* Track 1: ESP data collection (test, some values missing)
* Track 2: Static hum & temp measures
* Tracks 3-12: Static triangulation (10 different fixed points)
* Track 20: ESP data collection (latest version)
* Track 99: Test, to be discarded by the server

<img src="img/screen.jpg" alt="Screen UI" width="200" />
The track number is displayed at the bottom right of the screen.

## Library modifications

```
Give examples
```

## Server & Backend

Spaghetti:
Webmap:


## Built on

* [Tuino 1](https://tuino.io) - IoT reference development platform based on Arduino


## Authors

* **Micha Burger** - EPFL - [Github](https://github.com/michaburger)