433MHz Tx Rx
============

Arduino based 433MHz Tx and Rx combo using Manchester protocol

This matched pair of programs allow small packets of data (eg from 5 to 20 bytes) to be transmitted on a cheap 433MHz transmitter in OOK (On/Off Keying) mode to a cheap 433MHz receiver and using Arduino computers.

A data bit is made up of 2 500uS components.  It follows the Manchester convention that 500uS burst of RF followed by 500uS silence is the bit waveform for 1, and a silence of 500uS followed by a 500uS burst of signal is the waveform for a 0. This gives a data rate of 1000bps (in a burst say every 45 seconds, or when required).

The Transmitting program repeats the data packet 4 times over and the Receiver catches the packets, and when it receives 4 identical packets the contents of the packet are considered valid.  There are no complicated CRC's or checksums.

The programs have been modelled on the (now legacy) Bios or Thermor weather station protocol.

The programs are small and lightweight so could be deployed with nearly any of the Arduino variants.

People are encouraged to pull the examples apart and create their own versions or simply plug into the examples given. With these two programs it would not be hard to brew up your own custom system.

A more elaborate version of the Tx program is going to be developed that uses a simple arithmentic checksum and mimics the Oregon Scientific V3 protocol.  This will allow my own custom sensors to be developed that can be received alongside the standard OS sensors with the same basic Rx program.  I am looking to create original (ie Non OS sensor- ones they don't make at all) sensors.  For example a lightning detector, solar power, earth quake detector and even a bat call detector!!!  The core weather station detectors from Oregon Scientific will be the main, most important sensors though, the family will be extended around them!!!

These Weather stations by Oregon Scientific are highly recommended and appear to hit that sweet spot for economy and performance.  The perfect starting point for hacking your own set up.  Recommended gear!!

Cheers, Rob

Manchester convention asserted here:  lo->hi==1, hi->lo==0 (The reverse is also perfectly acceptable)
