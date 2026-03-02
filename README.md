LineageOS TV Device Tree for Orange Pi 3B V1.1
Works:
 - Boots
 - Goes through boot animation & setup to home screen
 - USB Gamepad
 - Wi-Fi
 - Bluetooth
Not working:
 - with GMS (with Mindthegapps somehow gms keeps crashing)
 - Display resolution is set to null which leads some crashes
 - weird first boot resolution, can be fixed by replugging the HDMI
 Other not tested.
Kernel:
We uses Armbian vendor kernel with some build files patch, in hopes of bringing Bluetooth and Wifi.
Wifi worked via getting firmware from (Infinix-X6816D_dump)[https://github.com/Veynamer/Infinix-X6816D_dump/tree/X6816D-user-12-SP1A.210812.016-235-release-keys/vendor/lib64].
And I tried to fix bluetooth by using `hciconfig_opi` and `hciattach_opi` from orangepi-build and worked.
