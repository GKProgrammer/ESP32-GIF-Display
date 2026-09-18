This ESP code helps you upload any 128x64px GIF (<2.8MB) to your ESP32 through your Wi-Fi network and display the GIF indefinitely as a loop on your SSD1306 OLED Screen.
You can access the ESP locally on the network using the address: esp32gif.local
The code also lets you clear the memory if ever want to do so.

IMPORTANT SIDE NOTES:
- Please make sure you install the required libraries mentioned in the REQUIREMENTS.txt file for the code to work properly.
- Make sure to set your own SSID and Password correctly where mentioned in the code as without them, the code will not work.
- If you upload a bigger sized gif (>128x64px), the gif will be cropped to the top left section of the gif, but it will still play.
- If you upload a smaller sized gif (<128x64px), the gif will play at the top left section of the OLED display.
