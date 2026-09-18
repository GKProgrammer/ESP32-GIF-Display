This ESP code helps you upload any 128x64px GIF (<2.8MB) to your ESP32 through your Wi-Fi network and display the GIF indefinitely as a loop on your SSD1306 OLED Screen.
You can access the ESP locally on the network using the address: esp32gif.local
The code also lets you clear the memory if ever want to do so.

SSD1306 OLED Display Pin Connections:

- GND -----> GND
- VCC -----> 3.3V
- SCL -----> GPIO22
- SDA -----> GPIO21

IMPORTANT SIDE NOTES:
- Before compiling and uploading the code, make sure that you have selected the "ESP32 Dev Module" as the Board.
- Change the Partitioning Scheme to "No OTA (1MB APP / 3MB SPIFFS)" as it allows for the higher 2.8MB file size for ESP32 DEVKIT Boards.
- Please make sure you install the required libraries mentioned in the REQUIREMENTS.txt file for the code to work properly.
- Make sure to set your own SSID and Password correctly where mentioned in the code as without them, the code will not work.
- If you upload a bigger sized gif (>128x64px), the gif will be cropped to the top left section of the gif, but it will still play.
- If you upload a smaller sized gif (<128x64px), the gif will play at the top left section of the OLED display.
