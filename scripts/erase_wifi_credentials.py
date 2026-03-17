#!/usr/bin/env python3
"""
Erases the WiFi SSID and password from NVS on the ESP32.

NVS namespace: wifi_manager
Keys erased:   ssid, pass

Usage:
    python erase_wifi_credentials.py [--port PORT]

Requires:
    pip install esptool esp-idf-nvs-partition-gen
    (or run from an activated ESP-IDF environment)
"""

import argparse
import subprocess
import sys
import tempfile
import os
import csv
import struct

NVS_NAMESPACE = "wifi_manager"
NVS_KEYS = ["ssid", "pass"]
NVS_PARTITION_SIZE = 0x6000  # 24 KB, matching partitions.csv
NVS_PARTITION_OFFSET = 0x9000


def find_port():
    """Try to auto-detect the ESP32 serial port."""
    import serial.tools.list_ports
    ports = list(serial.tools.list_ports.comports())
    # Check for Espressif USB VID (0x303A) first — matches ESP32-S3 native USB
    for p in ports:
        if "303A" in p.hwid.upper():
            return p.device
    # Then check common USB-to-serial chip descriptions
    for p in ports:
        if any(s in p.description for s in ("CP210", "CH340", "USB Serial")):
            return p.device
    if ports:
        return ports[0].device
    return None


def read_nvs_partition(port, baud=460800):
    """Read the NVS partition from the device into a temp file."""
    tmp = tempfile.NamedTemporaryFile(delete=False, suffix=".bin")
    tmp.close()
    cmd = [
        sys.executable, "-m", "esptool",
        "--port", port,
        "--baud", str(baud),
        "read_flash",
        hex(NVS_PARTITION_OFFSET),
        hex(NVS_PARTITION_SIZE),
        tmp.name,
    ]
    print(f"Reading NVS partition from device on {port}...")
    subprocess.check_call(cmd)
    return tmp.name


def write_nvs_partition(port, bin_path, baud=460800):
    """Write the NVS partition back to the device."""
    cmd = [
        sys.executable, "-m", "esptool",
        "--port", port,
        "--baud", str(baud),
        "write_flash",
        hex(NVS_PARTITION_OFFSET),
        bin_path,
    ]
    print("Writing updated NVS partition back to device...")
    subprocess.check_call(cmd)


def erase_keys_from_nvs(bin_path, namespace, keys):
    """
    Parse the raw NVS binary, zero-out entries matching the given
    namespace + keys, and write the modified binary back.

    NVS format (ESP-IDF):
      - Page size: 4096 bytes
      - Each page has a 32-byte header, then 126 entries of 32 bytes each,
        plus a 32-byte entry-state bitmap area.
      - Page header (32 bytes):
          offset 0:  uint32 page_state
          offset 4:  uint32 seq_number
          offset 8:  uint8  version
          offset 9:  unused[19]
          offset 28: uint32 crc32
      - After header, 32 bytes of entry-state bitmap (covers 126 entries, 2 bits each).
      - Then 126 entries, each 32 bytes:
          offset 0:  uint8  namespace_index
          offset 1:  uint8  data_type
          offset 2:  uint8  span (number of 32-byte entries this item uses)
          offset 3:  uint8  chunk_index
          offset 4:  uint32 crc32
          offset 8:  char   key[16]
          offset 24: varies (data area, 8 bytes)
    """
    PAGE_SIZE = 4096
    HEADER_SIZE = 32
    BITMAP_SIZE = 32
    ENTRY_SIZE = 32
    ENTRIES_PER_PAGE = 126

    with open(bin_path, "rb") as f:
        data = bytearray(f.read())

    num_pages = len(data) // PAGE_SIZE
    erased_keys = []

    # First pass: find namespace index for our namespace
    ns_index = None
    for page_idx in range(num_pages):
        page_offset = page_idx * PAGE_SIZE
        page_state = struct.unpack_from("<I", data, page_offset)[0]
        if page_state == 0xFFFFFFFF:
            continue  # empty page

        entries_offset = page_offset + HEADER_SIZE + BITMAP_SIZE
        for entry_idx in range(ENTRIES_PER_PAGE):
            entry_offset = entries_offset + entry_idx * ENTRY_SIZE
            if entry_offset + ENTRY_SIZE > len(data):
                break

            ns_idx = data[entry_offset]
            dtype = data[entry_offset + 1]
            span = data[entry_offset + 2]

            if ns_idx == 0 and dtype == 0x01:  # namespace entry
                key_bytes = data[entry_offset + 8 : entry_offset + 24]
                key_name = key_bytes.split(b"\x00")[0].decode("ascii", errors="ignore")
                if key_name == namespace:
                    ns_index = data[entry_offset + 24]  # the assigned index
                    break
        if ns_index is not None:
            break

    if ns_index is None:
        print(f"Namespace '{namespace}' not found in NVS. Nothing to erase.")
        return False

    print(f"Found namespace '{namespace}' with index {ns_index}")

    # Second pass: find and erase entries matching namespace + keys
    for page_idx in range(num_pages):
        page_offset = page_idx * PAGE_SIZE
        page_state = struct.unpack_from("<I", data, page_offset)[0]
        if page_state == 0xFFFFFFFF:
            continue

        entries_offset = page_offset + HEADER_SIZE + BITMAP_SIZE
        bitmap_offset = page_offset + HEADER_SIZE

        for entry_idx in range(ENTRIES_PER_PAGE):
            entry_offset = entries_offset + entry_idx * ENTRY_SIZE
            if entry_offset + ENTRY_SIZE > len(data):
                break

            entry_ns = data[entry_offset]
            dtype = data[entry_offset + 1]
            span = data[entry_offset + 2]

            if entry_ns != ns_index:
                continue

            key_bytes = data[entry_offset + 8 : entry_offset + 24]
            key_name = key_bytes.split(b"\x00")[0].decode("ascii", errors="ignore")

            if key_name in keys:
                print(f"  Erasing key '{key_name}' (span={span}) at page {page_idx}, entry {entry_idx}")
                # Mark entry (and its span) as erased by setting bitmap bits to "erased" (11)
                # and zeroing out the entry data
                for s in range(span):
                    idx = entry_idx + s
                    byte_pos = bitmap_offset + (idx // 4)
                    bit_pos = (idx % 4) * 2
                    # Set the 2-bit state to 0b11 (erased)
                    data[byte_pos] |= (0x03 << bit_pos)
                    # Zero out the entry
                    eo = entries_offset + idx * ENTRY_SIZE
                    data[eo : eo + ENTRY_SIZE] = b"\xFF" * ENTRY_SIZE

                erased_keys.append(key_name)

    if erased_keys:
        with open(bin_path, "wb") as f:
            f.write(data)
        print(f"Erased keys: {erased_keys}")
        return True
    else:
        print(f"Keys {keys} not found under namespace '{namespace}'.")
        return False


def main():
    parser = argparse.ArgumentParser(
        description="Erase WiFi SSID and password from ESP32 NVS"
    )
    parser.add_argument(
        "--port", "-p",
        help="Serial port (e.g. COM3, /dev/ttyUSB0). Auto-detected if omitted.",
    )
    parser.add_argument(
        "--baud", "-b",
        type=int,
        default=460800,
        help="Baud rate (default: 460800)",
    )
    args = parser.parse_args()

    port = args.port
    if not port:
        try:
            port = find_port()
        except ImportError:
            print("WARNING: pyserial not installed. Install it with: pip install pyserial")
            print("         Or specify the port manually with --port.")
    if not port:
        print("ERROR: Could not auto-detect serial port. Use --port to specify.")
        sys.exit(1)
    print(f"Using port: {port}")

    # Step 1: Read NVS partition from device
    nvs_bin = read_nvs_partition(port, args.baud)
    try:
        # Step 2: Erase the WiFi credential keys
        changed = erase_keys_from_nvs(nvs_bin, NVS_NAMESPACE, NVS_KEYS)

        if changed:
            # Step 3: Write modified NVS back
            write_nvs_partition(port, nvs_bin, args.baud)
            print("\nWiFi credentials erased successfully!")
            print("Reset the ESP32 for changes to take effect.")
        else:
            print("\nNo changes made.")
    finally:
        os.unlink(nvs_bin)


if __name__ == "__main__":
    main()