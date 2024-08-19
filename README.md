## lg4ff userspace driver for linux

This is an incomplete port of https://github.com/berarma/new-lg4ff to the userspace hidraw + uinput insterface

Supported devices:

- Logitech G25 (only tested on a g29 in g25 mode)
- Logitech G27 (only tested on a g29 in g27 mode)
- Logitech G29 (with the PC switch or the PS3 key bombo)

### When to use this at all

In most cases, you are recommended to use the new-lg4ff kernel driver instead. This driver is only useful when installing the kernel module is not very desired, either due to secure boot, or when using read only root distros like SteamOS or Fedora Silverblue.

The drawbacks of using this driver instead includes:

- Running the driver through hidraw + uinput could introduce more latency
- Some wheel support from new-lg4ff are yet to be implemented
- Some features from new-lg4ff are yet to be implemented

### Building

#### 1. Install gcc and hidapi-devel from your distro's package manager

Fedora:
```
dnf install gcc hidapi-devel
```

OpenSUSE:
```
zypper install gcc libhidapi-devel
```

Debian:
```
apt update
apt install gcc libhidapi-dev
```

#### 2. build binary
```
bash build.sh
```

### Preparations before using the driver

#### 1. To run pre-built releases, make sure you have hidapi library installed

Fedora:
```
dnf install hidapi
```

OpenSUSE:
```
zypper install libhidapi-hidraw0
```

Debian:
```
apt update
apt install libhidapi-hidraw0
```

#### 2. For driver mode to work, you'd need access to /dev/hidraw* of your wheel as well as /dev/uinput

You are recommended to NOT use this driver as root, instead, copy `60-lg4ff-userspace.rules` from this repository to `/etc/udev/rules.d/`, then run `sudo udevadm control --reload; sudo udevadm trigger` to set correct device node permissions

#### 3. For mode switching to work, you'd have to make sure the in-tree driver is not also performing mode switching to your wheel

Copy `logitech-hid.conf` to `/etc/modprobe.d/` then run `sudo modprobe -rv hid_logitech; sudo modprobe -v hid_logitech`

### Usage

```
usage: ./lg4ff_userspace <options>
listing devices:
  ./lg4ff_userspace -l
display this message:
  ./lg4ff_userspace -h
reboot wheel into another mode:
  ./lg4ff_userspace -m <g25/g27/g29> [-n device number in -l]
start driver on wheel:
  ./lg4ff_userspace -w [driver options]
  driver options:
  wheel number to start the driver on, defaults to 1:
    [-n <device number in -l>]
  gain, independent of application set gain, defaults to 65535:
    [-g <gain, 0-65535>]
  auto center gain, can be overriden by application, defaults to 0:
    [-a <auto center, 0-65535>]
  spring effect level, defaults to 30:
    [-s <spring level, 0-100>]
  damper effect level, defaults to 30:
    [-d <damper level, 0-100>]
  friction effect level, defaults to 30:
    [-f <friction level, 0-100>]
  hide all effects except for constant force from application:
    [-H]
  combine pedals, 0 for not combining any, 1 for combining gas and brake, 2 for combining gas and clutch, defaults to 0:
    [-c <0/1/2>]
```

### Usage Examples

#### List devices

```
$ ./lg4ff_userspace -l
Device 1:
 name: G29 Driving Force Racing Wheel
 vendor id: 0x046d
 product id: 0xc294
 mode: Drive Force
 backend path: /dev/hidraw1
```

#### Reboot device into compat modes

This step is required if your wheel is currently in Drive Force mode from using the PS3 compat button combo.

It is also useful for when one uses usb passthrough in RPCS3. Since RPCS3 does not support usb hotplugging, this tool can be used to change wheel mode before passing the device to RPCS3, for example setting a g29 to g27 before using it with RPCS3.

```
$ ./lg4ff_userspace -m g27
Device 1:
 name: G29 Driving Force Racing Wheel
 vendor id: 0x046d
 product id: 0xc294
 mode: Drive Force
 backend path: /dev/hidraw1
rebooting wheel 1 to G27
mode set command sent
```

#### Start driver

```
$ ./lg4ff_userspace -w
Device 1:
 name: G29 Driving Force Racing Wheel
 vendor id: 0x046d
 product id: 0xc29b
 mode: G27
 backend path: /dev/hidraw1
starting driver on wheel 1
gain: 65535
auto center: 0
spring level: 30
damper level: 30
friction level: 30
range: 900
hide effects: false
combine pedals: 0
sent range setting command for range 900
sent auto center disable command

```

### Disclaimer

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
