# ea_receiver
ea_receiver is a lightweight receiver for Elster EnergyAxis meters. It is designed to work with rtl_sdr compatible dongles, or any other source of complex unsigned 8-bit i/q samples. By itself, ea_receiver does not provide any insight into the received data. It simply validates the message checksum and writes it to stdout. The research behind ea_receiver is the foundation for a larger project which aims to provide full decoding of the EA_LAN protocol. Please see [the wiki](https://github.com/shaunhey/ea_receiver/wiki) for more information on the current state of research, or to contribute!

Please be aware that ea_receiver makes a number of tradeoffs which affects how well it receives. For example, ea_receiver can process multiple 400kHz channels simultaneously, but it does not perform a true channelization. Instead it simply decimates the incoming signal without any filtering. Therefore the resulting signal at 400ksps contains aliases of each channel. The downside of this is that if more than one transmission is received at a time, chances of any of them being properly demodulated are low. Since the EA_LAN protocol contains a 16-bit checksum, any corrupted messages are discarded. This design was chosen because the original use case was a Raspberry Pi in close proximity to the meters, and no antenna attached to the dongle. Future versions of this application (or its successor) will likely contain proper channelization, provided CPU usage can be kept to a reasonable level.

## Installation
```
# make install
```

## Usage
```
ea_receiver - A lightweight Elster EnergyAxis receiver
Usage: ea_receiver [options] FILE

  FILE        Unsigned 8-bit IQ file to process (or "-" for stdin)
  -c N        Number of 400kHz channels to receive (1-255, default 6)
```
####Examples
By default, a sample rate of 2.4Msps is expected. This provides for six 400kHz channels. Since the number of channels is even, the tuning freqency will actually be between the 3rd and 4th channels.
```
$ rtl_sdr -f 903.8e6 -s 2.4e6 - | ea_receiver -
  004e0980123456000000001c00009f...
```

Here, a sample rate of 2.0Msps is chosen. Since the number of channels is not even, the frequency specified is in the middle of the 3rd channel. Also, the "ts" command is shown here, as it can be chained to provide a nice timestamped output.
```
$ rtl_sdr -f 903.6e6 -s 2.0e6 - | ea_receiver -c 5 | ts
  Nov 23 22:45:09 004e0980123456000000001c00009f...
  Nov 23 22:45:37 004c0100654321801234561c000000...
```

Due to the simplicity of the receiver/demodulator in ea_receiver, it is very important that your frequency tuning is as accurate as possible. Most inexpensive rtl_sdr compatible dongles have some level of "ppm error". If you do not know the ppm error of your specific device, I recommend using the [kalibrate-rtl](https://github.com/steve-m/kalibrate-rtl) application. Out of the five devices I own, the ppm error ranges from 45 to 78.  The ppm error can be specified on the rtl_sdr command line like so:
```
$ rtl_sdr -f 903.6e6 -s 2.0e6 -p 45 - | ea_receiver
```

I also recommend experimenting with setting the gain explicitly in the rtl_sdr command line. In my experience, ea_receiver does better with a fixed gain (AGC off).

##Misc
Huge thanks to Michael Ossmann from Great Scott Gadgets for his great [video series on SDR](http://greatscottgadgets.com/sdr/).

Many thanks to Clayton Smith for publishing his GNURadio block [gr-elster](https://github.com/argilo/gr-elster). His research gave me a huge head start on this project.

##Contributing
Contributions are welcome! Please feel free to submit issues, pull requests, or email me directly. I am currently in the process of analyzing as many messages as possible to better understand the messaging format. I will be adding information to the wiki as soon as possible.
