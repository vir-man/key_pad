import os

MAIN_FILE = "c:\\Users\\Viral\\Documents\\Personal\\key_pad\\new_key_pad\\new_key_pad.ino"
BACKUP_FILE = "c:\\Users\\Viral\\Documents\\Personal\\key_pad\\new_key_pad\\new_key_pad_original.ino"
DIR = "c:\\Users\\Viral\\Documents\\Personal\\key_pad\\new_key_pad\\"

if not os.path.exists(BACKUP_FILE):
    import shutil
    shutil.copyfile(MAIN_FILE, BACKUP_FILE)

with open(BACKUP_FILE, 'r') as f:
    lines = f.readlines()

def get_lines(start_line, end_line):
    return lines[start_line-1:end_line]

def append_to_cpp(filename, start_line, end_line):
    path = os.path.join(DIR, filename)
    with open(path, 'a') as f:
        f.writelines(get_lines(start_line, end_line))

# Append missing Auth parts
append_to_cpp("auth_module.cpp", 2030, 2415)
append_to_cpp("auth_module.cpp", 4000, 4400) # fingerprint_fsm logic

# Append missing UI parts
append_to_cpp("ui_module.cpp", 1162, 1271)
append_to_cpp("ui_module.cpp", 1272, 1485)
append_to_cpp("ui_module.cpp", 1655, 1772)
append_to_cpp("ui_module.cpp", 2416, 3999)
append_to_cpp("ui_module.cpp", 4401, 4800)

# Append missing GSM parts
append_to_cpp("gsm_module.cpp", 1486, 1502)
append_to_cpp("gsm_module.cpp", 1773, 1798)
append_to_cpp("gsm_module.cpp", 5201, 6785)

print("Code blocks appended to CPP files successfully. You can now safely prune new_key_pad.ino!")
