import re
import os

MAIN_FILE = "c:\\Users\\Viral\\Documents\\Personal\\key_pad\\new_key_pad\\new_key_pad.ino"
BACKUP_FILE = "c:\\Users\\Viral\\Documents\\Personal\\key_pad\\new_key_pad\\new_key_pad.ino.bak"

if not os.path.exists(BACKUP_FILE):
    import shutil
    shutil.copyfile(MAIN_FILE, BACKUP_FILE)

with open(MAIN_FILE, "r") as f:
    code = f.read()

# 1. Strip Buzzer Module
buzzer_funcs = [
    r'void init_buzzer\(\) {.*?^}',
    r'void buzzer_on\([^)]*\) {.*?^}',
    r'void buzzer_task\(\) {.*?^}',
]
for pat in buzzer_funcs:
    code = re.sub(pat, '', code, flags=re.MULTILINE|re.DOTALL)

# 2. Strip RTC Module
rtc_funcs = [
    r'void rtc_begin\(\) {.*?^}',
    r'void update_date_time_from_rtc\(\) {.*?^}',
    r'void print_date_time\(\) {.*?^}',
    r'void rtc_task\(\) {.*?^}',
    r'byte bcdToDec\([^)]*\) {.*?^}',
    r'byte decToBcd\([^)]*\) {.*?^}',
    r'int get_day_of_week\([^)]*\) {.*?^}',
    r'uint32_t get_unix_time\([^)]*\) {.*?^}',
    r'bool is_holiday\([^)]*\) {.*?^}',
    r'void print_holidays\(\) {.*?^}',
]
for pat in rtc_funcs:
    code = re.sub(pat, '', code, flags=re.MULTILINE|re.DOTALL)

# 3. Strip Door Module
door_funcs = [
    r'void init_dc_motor\(\) {.*?^}',
    r'void dc_motor_stop\(\) {.*?^}',
    r'void dc_motor_run\([^)]*\) {.*?^}',
    r'bool is_door_open\(\) {.*?^}',
    r'bool is_door_close\(\) {.*?^}',
    r'void dc_motor_task\(\) {.*?^}',
]
for pat in door_funcs:
    code = re.sub(pat, '', code, flags=re.MULTILINE|re.DOTALL)

# 4. Strip Sensor Module
sensor_funcs = [
    r'void temp_sen_init\(\) {.*?^}',
    r'void temp_task\(\) {.*?^}',
]
for pat in sensor_funcs:
    code = re.sub(pat, '', code, flags=re.MULTILINE|re.DOTALL)

# Let's add the headers
header_includes = """
#include "config.h"
#include "logger.h"
#include "buzzer_module.h"
#include "rtc_module.h"
#include "door_module.h"
#include "sensor_module.h"
"""

if '#include "config.h"' not in code:
    code = code.replace('#include <Adafruit_Fingerprint.h>', '#include <Adafruit_Fingerprint.h>' + header_includes)

with open(MAIN_FILE, "w") as f:
    f.write(code)

print("Stripping complete")
