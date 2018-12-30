# arduino-alarm-clock

This is a significant change from the [original alarm clock](https://github.com/witnessmenow/arduino-alarm-clock) code published by [Brian Lough](http://blough.ie/bac/).

The following things have been changed:

* Fixed several of bugs
* Removed all the google maps code that calculates trip delays. I couldn't find the library Brian was using and I didn't need the functionality
* Added support for [Ring Tone Text Transfer Language - RTTTL](https://en.wikipedia.org/wiki/Ring_Tone_Transfer_Language) alarm sounds (see ringtones.rtttl file for hundreds of sounds you can use)
* Added support for a unique alarm time each day
* Removed snooze functionality
* The alarm rings forever until acknowledged
* Reduced frequency of NTP time requests to every 15 minutes (1 minute on error)
* Changed to 12 hour display format
* Used the left two vertical lines and the horizontal line in the left-most seven segment display to display AM/PM and global alarm disable
* Changed the meanings / functions of the buttons
 * left button shows the last byte of the clock's IP address
 * right button held for 2 seconds disables / enables global alarms (also plays the alarm sound when re-enabling alarms)
 * both buttons show the realtime value from the LDR
 * when the alarm is playing, either button instantly stops the alarm; there is no "snooze"
* Implemented threading using [Protothreads - Lightweight, Stackless Threads in C](http://dunkels.com/adam/pt/)
* changed the SPIFFS storage format to a binary representation of the alarmInfo structure
