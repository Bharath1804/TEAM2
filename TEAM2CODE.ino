#include <SPI.h>               // Include SPI library for ADC communication
#include <DHT.h>               // Include DHT library for temperature and humidity sensor
#include <Wire.h>              // Include Wire library for I2C communication
#include <Adafruit_GFX.h>      // Include Adafruit GFX library for graphics on OLED
#include <Adafruit_SSD1306.h>  // Include Adafruit SSD1306 library for OLED display
#include <SD.h>                // Include SD library for SD card operations
#include <SoftwareSerial.h>    // Include SoftwareSerial library for Bluetooth communication

#define SCREEN_WIDTH 128       // Define OLED screen width as 128 pixels
#define SCREEN_HEIGHT 64       // Define OLED screen height as 64 pixels
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1); // Create OLED display object

#define DHTPIN 6               // Define pin 6 for DHT sensor data line
#define DHTTYPE DHT11          // Define the DHT sensor type as DHT11
DHT dht(DHTPIN, DHTTYPE);      // Create a DHT object for temperature and humidity management

#define BUTTON_PIN 7           // Define pin 7 for the wake-up button

int CS_ADC = 2;                // Define pin 2 as ADC chip select (CS)
int CLK = 3;                   // Define pin 3 as ADC clock (CLK)
int DO = 4;                    // Define pin 4 as ADC data output (DO)
byte result;                   // Declare a variable to store ADC reading

#define SD_CS_PIN 53           // Define pin 53 for SD card chip select (CS)
File dataFile;                 // Declare a file object for managing SD card files

#define BT_RX 14               // Define pin 14 as Bluetooth RX
#define BT_TX 15               // Define pin 15 as Bluetooth TX
SoftwareSerial bluetooth(BT_RX, BT_TX); // Create Bluetooth serial object for communication

bool isSleeping = false;       // Initialize sleep mode flag to false
unsigned long lastActivity = 0; // Track last activity time in milliseconds
const unsigned long displayDuration = 5000; // Define display duration as 5000 ms

void setup() {
    Serial.begin(9600);            // Initialize Serial communication at 9600 baud rate
    bluetooth.begin(9600);         // Initialize Bluetooth communication at 9600 baud rate
    dht.begin();                   // Initialize the DHT sensor

    // Initialize OLED display and check for successful allocation
    Serial.println("Initializing OLED display...");
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("SSD1306 allocation failed")); // Print error if OLED initialization fails
        while (true);                                   // Halt program if OLED initialization fails
    }
    display.display();            // Show splash screen on OLED
    delay(2000);                  // Wait for 2 seconds
    display.clearDisplay();       // Clear the OLED screen

    pinMode(CS_ADC, OUTPUT);      // Set ADC CS pin as output
    pinMode(CLK, OUTPUT);         // Set ADC CLK pin as output
    pinMode(DO, INPUT);           // Set ADC DO pin as input
    digitalWrite(CS_ADC, HIGH);   // Deactivate ADC by setting CS high
    digitalWrite(CLK, LOW);       // Set ADC CLK low initially

    pinMode(BUTTON_PIN, INPUT_PULLUP); // Configure wake-up button pin with internal pull-up

    // Initialize SD card and check if successful
    if (!SD.begin(SD_CS_PIN)) {
        Serial.println("SD Card initialization failed!"); // Print error if SD card fails
        bluetooth.println("SD Card initialization failed!");
    } else {
        Serial.println("SD Card initialized."); // Print success message
        bluetooth.println("SD Card initialized.");
    }
}

void loop() {
    if (!isSleeping) {             // If not in sleep mode
        updateSensorData();        // Read and process sensor data
        unsigned long startDisplayTime = millis(); // Record start time of display
        while (millis() - startDisplayTime < displayDuration) { // Keep display active for set duration
            checkWake();           // Check for wake-up events
        }
        enterSleepMode();          // Enter sleep mode after display duration
    }
    checkWake();                   // Continuously check for wake-up events in sleep mode
}

void updateSensorData() {
    result = ADCread();                     // Read ADC data
    float voltage = (result / 255.0) * 5.0; // Convert ADC value to voltage
    float temperatureK = voltage * 100.0;  // Convert voltage to Kelvin temperature
    float temperatureC = temperatureK - 273.15; // Convert Kelvin to Celsius

    // Log ADC reading and LM335 temperature to Serial and Bluetooth
    Serial.print("ADC Raw Value: ");
    Serial.println(result);
    bluetooth.print("ADC Raw Value: ");
    bluetooth.println(result);

    Serial.print("LM335 Temp (C): ");
    Serial.println(temperatureC);
    bluetooth.print("LM335 Temp (C): ");
    bluetooth.println(temperatureC);

    float dhtTemperature = dht.readTemperature(); // Read temperature from DHT sensor
    // Log DHT temperature to Serial and Bluetooth
    Serial.print("DHT Temp (C): ");
    Serial.println(dhtTemperature);
    bluetooth.print("DHT Temp (C): ");
    bluetooth.println(dhtTemperature);

    // Display readings on OLED
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.print("LM335(C): ");
    display.println(temperatureC);
    display.print("DHT (C): ");
    display.println(dhtTemperature);
    display.display();

    logToSDCard(result, temperatureC, dhtTemperature); // Save data to SD card
    lastActivity = millis(); // Update last activity time
}

byte ADCread() {
    byte _byte = 0;                  // Initialize variable to store ADC data

    digitalWrite(CS_ADC, LOW);       // Activate ADC by setting CS low
    digitalWrite(CLK, HIGH);         // Generate clock pulse
    delayMicroseconds(2);            // Wait for 2 microseconds
    digitalWrite(CLK, LOW);          // Complete clock pulse
    delayMicroseconds(2);            // Wait for 2 microseconds

    // Read 8 bits from ADC
    for (int i = 7; i >= 0; i--) {
        digitalWrite(CLK, HIGH);     // Generate clock pulse
        delayMicroseconds(2);
        digitalWrite(CLK, LOW);      // Complete clock pulse
        delayMicroseconds(2);
        if (digitalRead(DO)) {       // Read ADC data line
            _byte |= (1 << i);       // Set the corresponding bit in the result
        }
    }
    digitalWrite(CS_ADC, HIGH);      // Deactivate ADC
    return _byte;                    // Return ADC value
}

void logToSDCard(byte adcValue, float lm335Temp, float dhtTemp) {
    dataFile = SD.open("log.csv", FILE_WRITE); // Open log file on SD card
    if (dataFile) {
        if (dataFile.size() == 0) { // Add header if file is empty
            dataFile.println("Timestamp,ADC Value,LM335 Temp (C),DHT Temp (C)");
        }
        unsigned long timestamp = millis(); // Get current time in milliseconds
        dataFile.print(timestamp);          // Write timestamp to file
        dataFile.print(",");
        dataFile.print(adcValue);           // Write ADC value to file
        dataFile.print(",");
        dataFile.print(lm335Temp);          // Write LM335 temperature to file
        dataFile.print(",");
        dataFile.println(dhtTemp);          // Write DHT temperature to file
        dataFile.close();                   // Close file
        Serial.println("Data logged to SD card."); // Notify data logged
        bluetooth.println("Data logged to SD card.");
    } else {
        Serial.println("Error opening log.csv!"); // Notify error
        bluetooth.println("Error opening log.csv!");
    }
}

void enterSleepMode() {
    if (!isSleeping) {           // Check if not already in sleep mode
        isSleeping = true;       // Set sleep mode flag
        display.clearDisplay();  // Clear OLED display
        display.display();       // Update OLED with blank screen
        Serial.println("Entering sleep mode"); // Notify sleep mode
        bluetooth.println("Entering sleep mode");
    }
}

void checkWake() {
    if (digitalRead(BUTTON_PIN) == LOW) { // Check if wake button is pressed
        isSleeping = false;              // Exit sleep mode
        lastActivity = millis();         // Reset inactivity timer
    }
}
