#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <LittleFS.h>
#include <AnimatedGIF.h>
#include <ESPmDNS.h> // --- NEW: Included for custom local domain ---

// OLED Settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// WiFi Settings - UPDATE THESE WIFI SETTINGS ACCORDING TO YOUR OWN WIFI NETWORK
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

WebServer server(80);
AnimatedGIF gif;
File gifFile;
File uploadFile;

const char* GIF_FILENAME = "/current.gif";
const char* TEMP_FILENAME = "/temp.gif";
bool deleteRequested = false; 
bool swapRequested = false;

const size_t MAX_FILE_SIZE = 2936012; 
size_t totalBytesWritten = 0;
bool uploadFailed = false;

// --- GIF Library Callbacks ---
void * GIFOpenFile(const char *fname, int32_t *pSize) {
  gifFile = LittleFS.open(fname, "r");
  if (gifFile) {
    *pSize = gifFile.size();
    return (void *)&gifFile;
  }
  return NULL;
}

void GIFCloseFile(void *pHandle) {
  File *f = static_cast<File *>(pHandle);
  if (f != NULL && *f) f->close();
}

int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen) {
  int32_t iBytesRead = iLen;
  File *f = static_cast<File *>(pFile->fHandle);
  
  if ((pFile->iSize - pFile->iPos) < iLen) {
    iBytesRead = pFile->iSize - pFile->iPos - 1;
  }
  if (iBytesRead <= 0) return 0;
  
  iBytesRead = f->read(pBuf, iBytesRead);
  pFile->iPos = f->position(); 
  return iBytesRead;
}

int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition) {
  File *f = static_cast<File *>(pFile->fHandle);
  f->seek(iPosition);
  pFile->iPos = f->position(); 
  return pFile->iPos;
}

// --- Drawing the GIF ---
void GIFDraw(GIFDRAW *pDraw) {
  uint8_t *s;
  uint16_t *pPal;
  int y, x;

  y = pDraw->iY + pDraw->y; 
  if (y >= SCREEN_HEIGHT) return; 

  s = pDraw->pPixels;
  pPal = (uint16_t *)pDraw->pPalette;

  for (x = 0; x < pDraw->iWidth; x++) {
    if (x + pDraw->iX >= SCREEN_WIDTH) break;

    if (pDraw->ucHasTransparency && *s == pDraw->ucTransparent) {
       s++;
       continue;
    }

    uint16_t color = pPal[*s++];
    uint8_t r = (color & 0xF800) >> 8;
    uint8_t g = (color & 0x07E0) >> 3;
    uint8_t b = (color & 0x001F) << 3;
    
    int brightness = (r * 77 + g * 150 + b * 29) >> 8;
    
    if (brightness > 127) {
      display.drawPixel(x + pDraw->iX, y, SSD1306_WHITE);
    } else {
      display.drawPixel(x + pDraw->iX, y, SSD1306_BLACK);
    }
  }
}

// --- Web Page HTML ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP32 GIF Uploader</title>
  <style>
    body { background: #222; color: #fff; font-family: Arial; text-align: center; padding: 30px; }
    form { background: #333; padding: 20px; border-radius: 10px; display: inline-block; margin-bottom: 20px;}
    input[type=file] { margin-bottom: 20px; }
    input[type=submit] { background: #2ecc71; color: white; border: none; padding: 10px 20px; cursor: pointer; border-radius: 5px; font-size: 16px; font-weight: bold;}
    .clear-btn { background: #e74c3c; margin-top: 10px; }
  </style>
</head>
<body>
  <h2>ESP32 GIF Player</h2>
  <p>Upload a 128x64 GIF (Max 2.8MB)</p>
  
  <form method="POST" action="/upload" enctype="multipart/form-data" id="uploadForm">
    <input type="file" name="update" accept=".gif" id="fileInput" required>
    <br>
    <input type="submit" value="Upload & Play">
  </form>
  
  <br>
  
  <form method="POST" action="/clear">
    <input type="submit" value="Clear ESP32 Memory" class="clear-btn">
  </form>

  <script>
    document.getElementById('uploadForm').onsubmit = function(event) {
      var fileInput = document.getElementById('fileInput');
      if (fileInput.files.length > 0) {
        var fileSize = fileInput.files[0].size;
        var maxSize = 2.8 * 1024 * 1024; // 2.8 MB in bytes
        if (fileSize > maxSize) {
          alert('Error: File is too large! Maximum allowed size is 2.8MB.');
          event.preventDefault(); // Immediately stops the upload
        }
      }
    };
  </script>
</body>
</html>
)rawliteral";

// --- Web Server Handlers ---
void handleRoot() {
  server.send(200, "text/html", index_html);
}

void handleUploadStart() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta http-equiv="refresh" content="3;url=/" />
  <title>Upload Status</title>
  <style>
    body { background: #222; color: #fff; font-family: Arial; text-align: center; padding: 50px; }
    .btn { background: #2ecc71; color: white; border: none; padding: 12px 24px; text-decoration: none; border-radius: 5px; font-size: 16px; font-weight: bold; display: inline-block; margin-top: 20px; }
    .btn-error { background: #e74c3c; }
    .redirect-text { color: #aaa; margin-top: 15px; font-style: italic; }
  </style>
</head>
<body>
  <h2>%TITLE%</h2>
  <p>%MESSAGE%</p>
  <p class="redirect-text">Redirecting in 3 seconds...</p>
  <a href='/' class='btn %BTN_CLASS%'>Go Back Now</a>
</body>
</html>
)rawliteral";

  // Swap out the placeholders based on success or failure
  if (uploadFailed) {
    html.replace("%TITLE%", "Upload Failed!");
    html.replace("%MESSAGE%", "The file exceeded the 2.8MB limit.");
    html.replace("%BTN_CLASS%", "btn-error");
    server.send(400, "text/html", html);
  } else {
    html.replace("%TITLE%", "Upload Complete!");
    html.replace("%MESSAGE%", "Look at the OLED screen.");
    html.replace("%BTN_CLASS%", "");
    server.send(200, "text/html", html);
  }
}

void handleFileUpload() {
  HTTPUpload& upload = server.upload();
  
  if (upload.status == UPLOAD_FILE_START) {
    deleteRequested = false; 
    swapRequested = false;
    uploadFailed = false;     
    totalBytesWritten = 0;    
    
    display.clearDisplay();
    display.setCursor(0,20);
    display.println("Uploading...");
    display.display();
    
    // Write to a temporary file so we don't crash the currently playing GIF
    uploadFile = LittleFS.open(TEMP_FILENAME, FILE_WRITE);
    
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFailed) return; 
    
    totalBytesWritten += upload.currentSize;
    
    if (totalBytesWritten > MAX_FILE_SIZE) {
      uploadFailed = true;
      if (uploadFile) {
        uploadFile.close();
      }
      LittleFS.remove(TEMP_FILENAME); // Delete the failed temp file
      Serial.println("Upload aborted: File exceeds 2.8MB.");
      return;
    }
    
    if (uploadFile) {
      uploadFile.write(upload.buf, upload.currentSize);
    }
    
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) {
      uploadFile.close();
    }
    
    if (uploadFailed) {
      display.clearDisplay();
      display.setCursor(0,20);
      display.println("File Too Large!");
      display.display();
      delay(3000); 
      display.clearDisplay();
      display.display();
    } else {
      swapRequested = true;
    }
  }
}

void handleClearMemory() {
  deleteRequested = true; 
  
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta http-equiv="refresh" content="3;url=/" />
  <title>Memory Cleared</title>
  <style>
    body { background: #222; color: #fff; font-family: Arial; text-align: center; padding: 50px; }
    .btn { background: #3498db; color: white; border: none; padding: 12px 24px; text-decoration: none; border-radius: 5px; font-size: 16px; font-weight: bold; display: inline-block; margin-top: 20px; }
    .redirect-text { color: #aaa; margin-top: 15px; font-style: italic; }
  </style>
</head>
<body>
  <h2>Memory Cleared!</h2>
  <p>The GIF has been successfully removed.</p>
  <p class="redirect-text">Redirecting in 3 seconds...</p>
  <a href='/' class='btn'>Go Back Now</a>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println(F("SSD1306 failed"));
    for(;;);
  }
  
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed");
    return;
  }
  
  gif.begin(LITTLE_ENDIAN_PIXELS);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Connecting to:");
  display.println(ssid);
  display.display();
  
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected!");

  if (!MDNS.begin("esp32gif")) {   // Set the hostname to "esp32gif" (YOU CAN CHANGE THIS TO YOUR PERSONAL PREFERENCE!)
    Serial.println("Error setting up MDNS responder!");
  } else {
    Serial.println("mDNS responder started");
  }

  server.on("/", HTTP_GET, handleRoot);
  server.on("/upload", HTTP_POST, handleUploadStart, handleFileUpload); 
  server.on("/clear", HTTP_POST, handleClearMemory);
  server.begin();

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Connected!");
  display.println("Browser Address:");
  display.println("esp32gif.local"); // Custom Web address (Local)
  display.display();
  
  delay(5000); 
  display.clearDisplay();
  display.display();
}

void loop() {
  server.handleClient();

  // 1. Handle Memory Clear Requests
  if (deleteRequested) {
    if (LittleFS.exists(GIF_FILENAME)) LittleFS.remove(GIF_FILENAME);
    if (LittleFS.exists(TEMP_FILENAME)) LittleFS.remove(TEMP_FILENAME);
    
    display.clearDisplay();
    display.setCursor(0, 20);
    display.println("Memory Cleared");
    display.display();
    delay(2000);
    display.clearDisplay();
    display.display();
    deleteRequested = false;
  }

  // 2. Handle File Swaps after a successful upload
  if (swapRequested) {
    if (LittleFS.exists(GIF_FILENAME)) {
      LittleFS.remove(GIF_FILENAME); // Delete the old GIF
    }
    LittleFS.rename(TEMP_FILENAME, GIF_FILENAME); // Promote the temp GIF to current
    swapRequested = false;
  }

  // 3. Play the GIF
  if (LittleFS.exists(GIF_FILENAME) && !deleteRequested && !swapRequested) {
    if (gif.open(GIF_FILENAME, GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw)) {
      
      // Loop through all frames
      while (gif.playFrame(true, NULL)) {
        server.handleClient(); 
        if (deleteRequested || swapRequested) {
          break; 
        }
        display.display();
      }
      
      gif.close(); // Gracefully closes the file, freeing it up for deletion/swapping
    }
  }
}