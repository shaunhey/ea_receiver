# ea_receiver
ea_receiver is a lightweight receiver for Elster EnergyAxis compatible meters. It is designed to work with rtl_sdr compatible dongles, or any other source of complex unsigned 8-bit i/q samples. By itself, ea_receiver does not provide any insight into the received data. It simply writes the messages to stdout. The research behind ea_receiver is the foundation for a larger project which aims to provide full decoding of the EA_LAN protocol. Please see the wiki for more information on the current state of research, or to contribute!

Please be aware that ea_receiver makes a number of tradeoffs which affects how well it receives. For example, ea_receiver can process multiple 400kHz channels simultaneously. It does not perform a true channelization, instead opting for simple decimation without any filtering. The resulting signal at 400ksps contains aliases of each channel. The downside of this is that if more than one transmission is received at a time, chances of any of them being properly demodulated are low. Since the EA_LAN protocol contains a 16-bit checksum, any corrupted messages are discarded. This design was chosen because the original use case was a Raspberry Pi in close proximity to the meters, and no antenna attached to the dongle.

## Installation
```
# make install
```
## Usage
```
ea_receiver - A lightweight Elster EnergyAxis Receiver
Usage: ea_receiver [options] FILE
  -c N         number of 400kHz channels to process
```
####Example
```
$ rtl_sdr -f 903.8e6 -s 2.4e6 - | ea_receiver -c 6 -
```
##Misc
Huge thanks to Michael Ossmann from Great Scott Gadgets for his great [video series on SDR](http://greatscottgadgets.com/sdr/).

Many thanks to Clayton Smith for publishing his GNURadio block [gr-elster](https://github.com/argilo/gr-elster). His research gave me a huge head start on this project.
