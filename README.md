# usbmuxd2

usbmuxd2 is a socket daemon that multiplexes connections between iOS devices and host computers over both USB and WiFi connections. It's a complete rewrite of the original usbmuxd with added WiFi support and improved stability. It's a drop in replacement for [usbmuxd](https://github.com/libimobiledevice/usbmuxd).

This is a fork of the original usbmuxd2 project by [@tihmstar](https://github.com/tihmstar). This fork supports direct network connections via IP address of iOS devices in order to use libimobiledevice instead of relying on mDNS / avahi discovery. This allows the software to work in more scenarios like VPN environments, where mDNS is usually not supported due to the lack of broadcast messages.

## Features

- USB device support for iOS devices
- WiFi device support (when compiled with libimobiledevice)
- Support for multiple connection backends:
  - Avahi-based WiFi discovery
  - mDNS-based WiFi discovery (macOS)
  - Direct WiFi connection via IP address
- Systemd integration
- Udev integration for automatic device detection
- Cross-platform support (Linux, macOS)

## Building

### How to build on Debian

Execute the following commands:

	mkdir -p /app/src

	apt-get update && apt-get upgrade -y
	apt-get install -y build-essential pkg-config checkinstall git autoconf automake
	apt-get install -y libtool-bin libssl-dev libcurl4-openssl-dev
	apt-get install -y libusb-1.0.0-dev libavahi-client-dev avahi-daemon avahi-utils

	cd /app/src

	git clone https://github.com/libimobiledevice/libplist.git \
	&& cd /app/src/libplist \
	&& ./autogen.sh \
	&& make \
	&& make install

	git clone https://github.com/libimobiledevice/libimobiledevice-glue.git \
	&& cd /app/src/libimobiledevice-glue \
	&& ./autogen.sh \
	&& make \
	&& make install

	git clone https://github.com/libimobiledevice/libtatsu.git \
	&& cd /app/src/libtatsu \
	&& ./autogen.sh \
	&& make \
	&& make install

	git clone https://github.com/libimobiledevice/libusbmuxd.git \
	&& cd /app/src/libusbmuxd \
	&& ./autogen.sh \
	&& make \
	&& make install

	git clone https://github.com/libimobiledevice/libimobiledevice.git \
	&& cd /app/src/libimobiledevice \
	&& ./autogen.sh \
	&& make \
	&& make install

	git clone https://github.com/tihmstar/libgeneral.git \
	&& cd /app/src/libgeneral \
	&& ./autogen.sh \
	&& make \
	&& make install

	git clone https://github.com/fosple/usbmuxd2.git \
	&& cd /app/src/usbmuxd2 \
	&& apt-get install -y clang \
	&& ./autogen.sh \
	&& ./configure CC=clang CXX=clang++ \
	&& make \
	&& make install

	apt-get install -y avahi-daemon dbus && apt-get clean

## Usage examples

## Avahi on Debian

    service dbus start
    service avahi-daemon start

    usbmuxd [options]

    ideviceinfo -n

## Direct IP connection via wifi on Debian

First connect the iOS device via USB, then run:

    usbmuxd

On iOS trust the device & enter PIN or use FaceID. 

Check if connection works:

    ideviceinfo

Should show info about your iOS device.

Disconnect from USB.

*Important*: Connect iOS device to Mac Finder / Win iTunes and enable Wifi Sync:
1. Connect iOS device to Mac via USB
2. Go to Finder and open iOS device
3. Check checkbox "Show this iphone when on wifi"
4. Click "Apply"

Make sure iOS device is on the network and able to accept network connections (screen powered on), run:

    usbmuxd -c 192.168.0.1 --pair-record-id 00000000-000A000000000000

- 192.168.0.1: IP address of your iOS device
- 00000000-000A000000000000: Filename of pairing information plist file (w/o `.plist`), created during first pairing via USB for your iOS device. E.g. on Debian located in: `/var/lib/lockdown/00000000-000A000000000000.plist`

Check if connection works:

    ideviceinfo -n

Should return device info. It's important to add the `-n` as this requests the info via the network.

## Options

See help for other options of usbmuxd.

    usbmuxd -h

Returns:

    Usage: usbmuxd [OPTIONS]
    Expose a socket to multiplex connections from and to iOS devices.

    -h, --help			Print this message.
    -d, --daemonize		Do daemonize
    -l, --logfile=LOGFILE		Log (append) to LOGFILE instead of stderr or syslog.
    -p, --no-preflight		Disable lockdownd preflight on new device.
    -s, --systemd			Run in systemd operation mode (implies -z and -f).
    -U, --user USER		Change to this user after startup (needs USB privileges).
    -v, --verbose			Be verbose (use twice or more to increase).
    -V, --version			Print version information and exit.
    -z, --enable-exit		Enable "--exit" request from other instances and exit
                            automatically if no device is attached.
    -x, --exit			Notify a running instance to exit if there are no devices
                            connected (sends SIGUSR1 to running instance) and exit.
    -X, --force-exit		Notify a running instance to exit even if there are still
                            devices connected (always works) and exit.
    -c, --connect IP		Connect directly to device at IP address
        --debug			Enable debug logging
        --allow-heartless-wifi	Allow WIFI devices without heartbeat to be listed (needed for WIFI pairing)
        --no-usb			Do not start USBDeviceManager
        --no-wifi			Do not start WIFIDeviceManager
        --pair-record-id ID		Set the pair record ID for the connection

## License

This project is licensed under the GNU Lesser General Public License v3.0 - see the [LICENSE](LICENSE) file for details.

## Credits

Originally developed by [@tihmstar](https://github.com/tihmstar)

Direct IP addition by [@fosple](https://github.com/fosple)

## Note

This is a rewrite of the original usbmuxd with significant improvements and additional features. It's designed to be a drop-in replacement for the original usbmuxd.
