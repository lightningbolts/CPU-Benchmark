import platform
import os

if platform.system() == "Windows":
    os_info = os.popen("systeminfo | findstr /C:OS").read()
    os_info = os_info[os_info.find(":") + 2:]
    os_info = os_info.replace("\n", "")
    os_info = os_info.replace("\r", "")
    # Organize the output
    os_info = os_info[:os_info.find("Version:") - 1]
    os_info = os_info.replace("                  ", "")

print(os_info)