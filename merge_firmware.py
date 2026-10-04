# PlatformIO extra script: merge bootloader + partition table + app (+ LittleFS image)
# into one file that Wokwi can load (.pio/build/<env>/firmware.merged.bin).
# Based on the script in the Wokwi docs: https://docs.wokwi.com/vscode/platformio
Import("env")
from os.path import join, isfile, getsize

# Offset of the spiffs/littlefs partition. 0x290000 is the default 4MB Arduino
# partition table (the esp32-c3-devkitm-1 default). Change it if you use a custom
# partitions.csv.
FS_OFFSET = "0x290000"

def merge_firmware(source, target, env):
    board = env.BoardConfig()
    build_dir = env.subst("$BUILD_DIR")
    merged = join(build_dir, "firmware.merged.bin")
    esptool = join(env.PioPlatform().get_package_dir("tool-esptoolpy"), "esptool.py")
    images = [(offset, env.subst(path)) for offset, path in env.get("FLASH_EXTRA_IMAGES", [])]
    images.append((env.subst("$ESP32_APP_OFFSET"), str(target[0])))
    fs_image = join(build_dir, env.subst("$ESP32_FS_IMAGE_NAME") + ".bin")
    if FS_OFFSET and isfile(fs_image):
        images.append((FS_OFFSET, fs_image))
    else:
        print("merge_firmware: no filesystem image found at %s - run 'pio run -t buildfs' first" % fs_image)
    cmd = [
        '"$PYTHONEXE"', '"%s"' % esptool,
        "--chip", board.get("build.mcu", "esp32"),
        "merge_bin", "-o", '"%s"' % merged,
        "--flash_mode", board.get("build.flash_mode", "dio"),
        "--flash_size", board.get("upload.flash_size", "4MB"),
    ]
    for offset, path in images:
        # offsets may be strings ("0x1000") or ints, depending on the framework
        cmd += [offset if isinstance(offset, str) else hex(offset), '"%s"' % path]
    env.Execute(env.VerboseAction(" ".join(cmd), "Merging firmware images into %s" % merged))

    # Diagnostics: a merged image that contains the filesystem ends at the end of the
    # spiffs partition (FS_OFFSET + 0x160000 = 4128768 bytes for the default table).
    # A much smaller file means the filesystem was NOT merged, and LittleFS will fail
    # to mount with "Corrupted dir pair".
    included = bool(FS_OFFSET and isfile(fs_image))
    if isfile(merged):
        print("merge_firmware: %s = %d bytes, filesystem %s" %
              (merged, getsize(merged), "INCLUDED at " + str(FS_OFFSET) if included else "NOT included"))

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_firmware)
