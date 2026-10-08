# Frozen modules for the Waveshare ESP32-C6-ePaper-1.54 clock firmware.

# Port defaults: _boot.py (VFS setup), inisetup.py, ntptime, etc.
freeze("$(PORT_DIR)/modules")
include("$(MPY_DIR)/extmod/asyncio")

# The clock application (main.py is frozen too, so a fresh board runs the clock).
# A main.py placed on the filesystem takes precedence during development.
freeze("$(BOARD_DIR)/../../../python")
