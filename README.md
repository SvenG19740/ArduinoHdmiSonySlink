# ArduinoHdmiSonySlink
This repo is dedicated to control an AV Receiver from Sony via S-Link receiving the control signals from the HDMI port of the TV set.

Build the hardware using some transistors, resistors, capacitors, a Arduino Mega, a HDMI breakout board and a 3,5 mm mono plug. Connect the mono plug to your Sony amplifier / receiver and the HDMI break-out board to your TV set. Depending on the HDMI port at the TV the address has to be configured in the code. If the Arduino gets recognized select "Arduino" as loudspeaker output. You need a power supply for the Arduino.

Disclaimer: (will solve that later) I cannot remember exactly but I think I had to change the CEC library to get the code working. Will fix that soon and reply herein.

What the code does:
1) Register the Arduino as an Audio device on the CEC bus (HDMI) and allocate an address.
2) Capture commands on the CEC bus as Power On, Power Off, Mute, Volume Up, Volume Down.
3) Send appropriate commands to the Sony Slink interface.

Result:
You use your TV set remote control turning on the TV and the receiver gets powered on. If you change or mute the volume on the remote control the receiver changes the volume or mutes it.

The idea behind is that I have a legacy Sony AV Receiver (Sony STR-DB830) capable receiving digital sound using the optical input and a TV set from Samsung with HDMI ports and opt-out for sound. I did not want to spend (again) money for a amplifier having HDMI ports. I do not want to use several remote controls. I do not want to use programmable remote controls. The concept sending IR codes to the amplifier I do not like since you have to place the IR diode somewhere. So my idea was to catch the signals on HDMI and use the Slink interface from Sony to control the receiver. I started with a Raspi because it has an HDMI port and a library capable reading CEC on HDMI. But I wanted to have it on the Arduino.

To Do:
- semi-professional PCB
- using an Arduino Nano instead of Mega
- using the +5 V of the HDMI port instead of a power supply
- housing
- code optimization, e.g. deleting debug messages
- increasing robustness of the code

Sources:

https://github.com/robho/sony_slink

https://github.com/floe/CEC

Support: using a chatbot writing and debugging the code
