# Scripts

## erase_wifi_credentials.py

Erases only the WiFi SSID and password stored in the ESP32 NVS partition (namespace: `wifi_manager`, keys: `ssid`, `pass`). All other NVS data is preserved.

### Install dependencies

```bash
pip install -r scripts/requirements.txt
```

### Usage

Run from the **project root** directory:

```bash
# Auto-detect serial port
python scripts/erase_wifi_credentials.py

# Specify port manually
python scripts/erase_wifi_credentials.py --port COM5

# Custom baud rate
python scripts/erase_wifi_credentials.py --port COM5 --baud 115200
```

Or from the `scripts/` directory:

```bash
python erase_wifi_credentials.py
```

Reset the ESP32 after running the script for changes to take effect.
