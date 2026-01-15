import requests
import os
import sys

# Configuration
VERSION_FILE = 'version.txt'
FIRMWARE_PATH = '.pio/build/esp32-devkit/firmware.bin'
UPLOAD_URL = 'https://lighting-firmware-server.onrender.com/api/upload?force=true'

def get_next_version():
    if not os.path.exists(VERSION_FILE):
        print(f"Error: {VERSION_FILE} not found!")
        sys.exit(1)
        
    with open(VERSION_FILE, 'r') as f:
        try:
            current = int(f.read().strip())
        except ValueError:
            print(f"Error: Invalid version number in {VERSION_FILE}")
            sys.exit(1)
            
    next_ver = current + 1
    
    with open(VERSION_FILE, 'w') as f:
        f.write(str(next_ver))
        
    return f"1.1.{next_ver}"

def upload_firmware(version):
    if not os.path.exists(FIRMWARE_PATH):
        print(f"Error: Firmware not found at {FIRMWARE_PATH}")
        print("Did you forget to build the project?")
        sys.exit(1)
        
    print(f"Uploading version {version} from {FIRMWARE_PATH}...")
    
    try:
        with open(FIRMWARE_PATH, 'rb') as fw:
            files = {
                'firmware': ('firmware.bin', fw, 'application/octet-stream')
            }
            data = {
                'version': version
            }
            
            response = requests.post(UPLOAD_URL, files=files, data=data)
            
            print(f"Status Code: {response.status_code}")
            print(f"Response: {response.text}")
            
            if response.status_code == 200 or response.status_code == 201:
                print("✅ Upload successful!")
            else:
                print("❌ Upload failed!")
                
    except Exception as e:
        print(f"Error uploading: {e}")

if __name__ == '__main__':
    version = get_next_version()
    print(f"New Version: {version}")
    upload_firmware(version)
