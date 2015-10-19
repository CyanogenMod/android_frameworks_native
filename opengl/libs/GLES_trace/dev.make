## NOTE
## This file is used for development purposes only. It is not used by the build system.

sync:
	adb root
	adb remount
	adb shell stop
	adb sync
	adb shell start
