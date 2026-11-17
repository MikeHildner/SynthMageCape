# Runs before every build (see extra_scripts in platformio.ini).
# Writes include/BuildStamp.h with the time of this build, so the firmware can
# say which build it is. That is how an update over Wi-Fi is confirmed: the
# status page shows the new time.
import datetime
import os

Import("env")  # noqa: F821  (provided by PlatformIO)

path = os.path.join(env["PROJECT_INCLUDE_DIR"], "BuildStamp.h")  # noqa: F821
stamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
with open(path, "w", encoding="utf-8") as f:
    f.write('#pragma once\n#define BUILD_STAMP "%s"\n' % stamp)
