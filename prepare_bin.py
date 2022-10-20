Import("env", "projenv")
import os
import shutil
def before_upload(source, target, env):
    print("--------- Save Firmware before upload -------------------")
    
    # Get source dir (.pio/name_env/)
    source = env.get("PROJECT_BUILD_DIR") + "/" + env.get("PIOENV")
    # Save at destination docs/bins
    destination = os.getcwd() + "/docs/bins" + "/" #+ env.get("PIOENV")
    shutil.copyfile(source + "/firmware.bin", destination + "/firmware.bin")
    shutil.copyfile(source + "/partitions.bin", destination + "/partitions.bin")
    shutil.copyfile(source + "/spiffs.bin", destination + "/spiffs.bin")
    print(source)
    print(destination)
    print("---------------------------------------------------------")

env.AddPreAction("buildfs", before_upload)

