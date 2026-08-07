
# Gilboa Water Temperature Monitor

## RECEIVER

### Setting up the WiFi Connection of the Receiver

1. Power up the Receiver.
2. On a PC that is already connected to the WiFi network, change the WiFi connection to the Receiver temporary WiFi SSID **"WaterGaugeReceiver"** with the password:

   ```
   password123
   ```

3. A WiFiManager screen will appear. If it does not, open a web browser and navigate to:

   ```
   http://192.168.4.1
   ```

4. Press **Configure WiFi**.
5. If a list of networks is displayed:
   - Select the appropriate network.
   - Enter the SSID and password.
   - Save the settings.
6. Once the Receiver accepts the new network credentials, it will obtain an IP address from the selected network.
7. The assigned IP address will be displayed on the Receiver display.

---

### WiFi Reset Button

A WiFi reset button is located on the back of the Receiver.

To reset:

1. Insert a small tool (such as a toothpick) through the access hole.
2. Press until the display indicates that a WiFi reset has occurred.
3. Repeat the **Setting up the WiFi Connection of the Receiver** procedure above.

---

# Water Detected Alarm

The temperature probes are located inside a sealed vinyl tube extending from the surface to **130 feet** underwater.

The tube keeps the electronics dry.

At the bottom of the tube are two water sensors:

- **Lower sensor** — warning sensor
- **Upper sensor** — final alarm sensor (approximately 2 inches above the lower sensor)

When water reaches either sensor:

1. The sensor notifies the Sender located at the surface.
2. During its normal wake-up cycle the Sender transmits the alarm to the Receiver.
3. The Receiver sends an email or text message to every address listed in the **CONFIG** screen.

---

## Sending Text Messages by Email

Use the following email-to-SMS gateway addresses.

| Carrier | Address |
|---------|----------|
| AT&T | `1234567890@txt.att.net` |
| Verizon | `1234567890@vtext.com` |
| Verizon (messages over 160 characters) | `1234567890@vzwpix.com` |
| T-Mobile | `1234567890@tmomail.net` |
| Sprint | `1234567890@messaging.sprintpcs.com` |

---

# Sender and Receiver Communication

After waking from sleep, the Sender performs the following sequence.

## 1. Send Temperature

Format

```text
T,<thermocouple ROM>,<temperature °F>
```

Example

```text
T,28 DB A7 B1 00 00 00 5E,37.2
```

---

## 2. Delay

```
200 ms
```

---

## 3. Send Sender Status

Format

```text
D,<battery voltage>,<version>,<minutes asleep>,<sleep setpoint>,
<OLED enabled>,<reserved>,<bottom water sensor>,<top water sensor>
```

Example

```text
D,4.08,v1.2.01,10,0,0,0,N,N
```

---

## 4. Delay

```
200 ms
```

---

## 5. Send Complete Command

```text
C
```

---

## Receiver Response

The Receiver replies with:

```text
V,<sleep time>,<OLED disable>,<debug>
```

Example

```text
V,20,0,0
```

---

# Water Detection Email Recipients

If either of these values is received as **Y**:

- Water detected at bottom pins
- Water detected at top pins

the Receiver immediately sends notifications to every recipient in the **Water Detection Email Recipients** list on the configuration screen.

---

# Thermocouple Failure Detection

## Individual Sensor Failure

If a thermocouple fails but the OneWire bus is still operating, that thermocouple reports a **negative temperature**.

---

## OneWire Bus Failure

If the OneWire communication bus fails:

- No thermocouples can be read.
- The Receiver sends a fault message to the primary contact.
- The message reports that the **OneWire bus has faulted**.
- When the fault is corrected, another message is sent containing the restored temperatures.

---

# Telegram Setup

Install the free Telegram application.

Search for and connect to:

```
Gilboa_WaterTemp_bot
```

This allows monitoring the Receiver remotely instead of using the local WiFi interface.

---

## Telegram Commands

### `/status`

Displays:

- Receiver IP address
- Sender temperatures (air temperature plus every 10 ft)
- Sender battery voltage
- Sender version
- Sender CPU temperature
- Sender watchdog counter (current/previous)
- Sender watchdog enabled state
- Sleep timer status
- Receiver version
- Receiver CPU temperature
- Water detection status
- HTTP status code from the last POST request
- Sender OLED mode
- Sender debug mode

---

### `/rom`

Displays ROM addresses of every thermocouple.

---

### `/debug-on`

Enable Sender debug mode.

---

### `/debug-off`

Disable Sender debug mode.

---

### `/WDT-on`

Enable Receiver watchdog timer.

---

### `/WDT-off`

Disable Receiver watchdog timer.

---

### `/ota`

Display OTA partition information.

---

### `/_UPDATE`

Downloads and installs the latest firmware from GitHub.

---

### `/_RESET`

Reset the Receiver.

---

### `/help`

Display safe commands.

---

### `/start`

Same as `/help`.

---

### `?`

Same as `/help`.

---

### `/superhelp`

Displays every available command.

---

## Debug Receiver

Ground **GPIO 4** when using the debugging Receiver hardware.

This changes the Telegram bot name to:

```
Gilboa_WaterTemp_Debug_bot
```

allowing debugging without interfering with the production Receiver.

---

# Uploading New Programs to the Sender (WiFi OTA)

1. Be within WiFi range of the Sender.
2. Move the magnetic switch dongle from its storage position to the active position beneath the solar panel.
3. Connect to WiFi:

```
SSID: Gilboa_Sender
Password: Gilboa_Sender
```

4. Open:

```
http://192.168.4.2/update
```

5. Select **Choose File**.
6. Select the desired `.bin` firmware.
7. Press **Update**.
8. The Sender uploads the firmware and automatically reboots.
9. WiFi disconnects during reboot.
10. Move the magnetic switch back to its storage position.

---

# Uploading New Programs to the Receiver Using Android (USB)

Requirements:

- Android device
- ESPFlash application
- USB-C to USB-C cable

## Determine the Active OTA Slot

1. Open Telegram.
2. Connect to:

```
Gilboa_WaterTemp_bot
```

3. Run:

```
/ota
```

4. Note:

- Running partition
- Slot
- Offset

Typical offsets:

```
0x10000
0x340000
```

---

## Flash Firmware

1. Connect the USB cable.
2. Open ESPFlash.
3. Select the **Flash** icon.
4. Choose the firmware `.bin`.
5. Enter the OTA offset.
6. Select the ESP32 device.
7. Set baud rate:

```
115200
```

8. Upload.

ESPFlash can also monitor the serial output using the **Monitor** button.

---

# Uploading New Programs to the Receiver Using GitHub

Remote firmware updates can be performed through Telegram.

1. Connect to:

```
Gilboa_WaterTemp
```

2. Send:

```
/_UPDATE
```

The Receiver will:

1. Download `Receiver_firmware.bin`
2. Flash the firmware
3. Restart automatically

---

# GitHub Repository

Repository name:

```
Gilboa_Water_Temp
```

Directory structure:

```text
Documentation/
Freecad/
KiCAD/
Visual_Studio_Code/
```

| Directory | Contents |
|-----------|----------|
| Documentation | Documentation |
| Freecad | 3D printable designs |
| KiCAD | Schematics |
| Visual_Studio_Code | Sender and Receiver C++ source code |

---

# Creating a New Release

After modifying files:

1. Save changes.
2. Stage all files.
3. Commit with a commit message.
4. In GitHub select **Release**.
5. Create a new tag.

Example:

```text
Release_4
```

6. Enter a release name.

Example:

```text
Release 4 - Receiver v1.3.02 - Sender v1.2.03
```

7. Attach the firmware binaries:

- `Receiver_firmware.bin`
- `Sender_firmware.bin`

Duplicate firmware.bin and rename the uploaded binaries to match these filenames before publishing the release.
