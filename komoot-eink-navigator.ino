#include "symbols.h"

/**
   adapted from palto42's code for the ssd1306 display https://github.com/palto42/komoot-navi
*/

#include "BLEDevice.h"
// next two lines added for pairing
#include <BLEUtils.h>
#include <BLEServer.h>

#define LILYGO_T5_V213
#include <boards.h>
#include "esp_adc_cal.h"
#include <driver/adc.h>
#include <GxEPD.h>
#include <GxDEPG0213BN/GxDEPG0213BN.h>    // 2.13" b/w  form DKE GROUP
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>

// next two lines added for pairing
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// copy constructor for your e-paper from GxEPD2_Example.ino, and for AVR needed #defines
#define MAX_DISPLAY_BUFFER_SIZE 800 // 
#define MAX_HEIGHT(EPD) (EPD::HEIGHT <= MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8) ? EPD::HEIGHT : MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8))

GxIO_Class io(SPI,  EPD_CS, EPD_DC,  EPD_RSET);
GxEPD_Class display(io, EPD_RSET, EPD_BUSY);


std::string value = "Start";
int timer = 0 ;
// The remote service we wish to connect to.
static BLEUUID serviceUUID("71C1E128-D92F-4FA8-A2B2-0F171DB3436C");
// The characteristic of the remote service we are interested in.
static BLEUUID    charUUID("503DD605-9BCB-4F6E-B235-270A57483026");

static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;
static BLEAdvertisedDevice* myDevice;

int16_t connectTextBoundsX, connectTextBoundsY;
uint16_t connectTextBoundsW, connectTextBoundsH;
const char connectText[] = "Waiting...";

int16_t connectedTextBoundsX, connectedTextBoundsY;
uint16_t connectedTextBoundsW, connectedTextBoundsH;
const char connectedText[] = "Connected";

const int battPin = 35; // A2=2 A6=34
unsigned int raw = 0;
float volt = 0.0;
// ESP32 ADV is a bit non-linear
const float vScale1 = 579; // divider for higher voltage range
const float vScale2 = 689; // divider for lower voltage range

long interval = 300000;  // interval to display battery voltage 5mins should be sufficient
long previousMillis = 0; // used to time battery update

// distance and streets
std::string old_street = "";
uint8_t dir = 255;
uint32_t dist2 = 4294967295;
std::string street;
std::string firstWord;
std::string old_firstWord;
bool updated;
bool updated_dist = 0;
bool updated_dir = 0;
bool updated_street = 0;

double mapf(double val, double in_min, double in_max, double out_min, double out_max) {
  return (val - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  //  Serial.print("Notify callback for characteristic ");
  //  Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
  //  Serial.print(" of data length ");
  //  Serial.println(length);
  //  Serial.print("data: ");
  //  Serial.println((char*)pData);
}

class MyClientCallback : public BLEClientCallbacks {

    void onConnect(BLEClient* pclient) {
    }

    void onDisconnect(BLEClient* pclient) {
      connected = false;
      Serial.println("onDisconnect");
    }
};

bool connectToServer() {

  Serial.print("Forming a connection to ");
  Serial.println(myDevice->getAddress().toString().c_str());

  BLEClient* pClient = BLEDevice::createClient();
  Serial.println(" - Created client");

  pClient->setClientCallbacks(new MyClientCallback());

  // Connect to the remove BLE Server.
  pClient->connect(myDevice);  // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
  Serial.println(" - Connected to server");

  // Obtain a reference to the service we are after in the remote BLE server.
  BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr) {
    Serial.print("Failed to find our service UUID: ");
    Serial.println(serviceUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println(" - Found our service");


  // Obtain a reference to the characteristic in the service of the remote BLE server.
  pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
  if (pRemoteCharacteristic == nullptr) {
    Serial.print("Failed to find our characteristic UUID: ");
    Serial.println(charUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println(" - Found our characteristic");

  // Read the value of the characteristic.
  if (pRemoteCharacteristic->canRead()) {
    std::string value = pRemoteCharacteristic->readValue();
    if (pRemoteCharacteristic->canNotify()) {
      pRemoteCharacteristic->registerForNotify(notifyCallback);
      Serial.println(" - Registered for notifications");

      connected = true;
      return true;
      //display.fillRect(0, 0, GxEPD_WIDTH, GxEPD_HEIGHT, GxEPD_WHITE );
    }
    Serial.println("Failed to register for notifications");
  } else {
    Serial.println("Failed to read our characteristic");
  }

  pClient->disconnect();
  return false;
}
/**
   Scan for BLE servers and find the first one that advertises the service we are looking for.
*/
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    /**
        Called for each advertising BLE server.
    */
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      Serial.print("BLE Advertised Device found: ");
      Serial.println(advertisedDevice.toString().c_str());

      // We have found a device, let us now see if it contains the service we are looking for.
      if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {
        BLEDevice::getScan()->stop();
        myDevice = new BLEAdvertisedDevice(advertisedDevice);
        doConnect = true;
        doScan = true;
      }
    }
};

void showPartialUpdate_dir(uint8_t dir) {   // <--------------------------- DIRECTION
  display.fillRect(0, 0, 64, 64, GxEPD_WHITE);
  display.updateWindow(0, 0, 220, 120, false);
  display.drawBitmap(5, 5, symbols[dir].bitmap, 60, 60, 0);
  display.updateWindow(0, 0, 64, 64, true);
  updated_dir = 1;
}

void showPartialUpdate_street(std::string street, std::string old_street ) { // <-------- STREET
  display.setTextColor(GxEPD_BLACK);
  display.setFont(&FreeSansBold9pt7b);
  display.updateWindow(0, 0, 220, 120, false);
  display.fillRect(10, 66, 239, 54, GxEPD_WHITE);
  display.setCursor(10, 85);
  display.print(street.c_str());
  display.setCursor(10, 115);
  display.print(old_street.c_str());
  updated_street = 1;
  display.updateWindow(10, 66, 239, 54, true);
}

void showPartialUpdate_dist(uint32_t dist) { //<--------------------------------- DISTANCE

  display.setFont(&FreeSansBold18pt7b);
  display.setTextColor(GxEPD_BLACK);
  display.updateWindow(0, 0, 220, 120, false);
  display.fillRect    (85, 33, 120, 30, GxEPD_WHITE);
  display.setCursor(105, 57);
  display.print(dist);
  display.println("m");
  display.updateWindow(85, 33, 120, 30, true);
  updated_dist = 1;

}

void Battery_check(void) {
  raw  = (float) analogRead(battPin);
  volt = raw / vScale1;
  Serial.print ("Battery = ");
  Serial.println (volt);
  Serial.print ("Raw = ");
  Serial.println (raw);

  display.updateWindow(0, 0, 220, 120, false);
  display.fillRect (200, 0, 50, 20, GxEPD_WHITE);
  display.setFont(&FreeSansBold9pt7b);
  display.setTextColor(GxEPD_BLACK);
  display.setCursor(200, 15);
  int VbatP = mapf(volt, 3.0, 4.26, 0, 100); // vbat en pourcentage
  display.print(VbatP);
  display.print("%");
  display.updateWindow(200, 0, 50, 20, true);

}

void setup() {
  // enable debug output
  Serial.begin(115200);

  // Battery voltage
  pinMode(battPin, INPUT);
  esp_adc_cal_characteristics_t adc_chars;
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize((adc_unit_t)ADC_UNIT_1, (adc_atten_t)ADC_ATTEN_DB_2_5, (adc_bits_width_t)ADC_WIDTH_BIT_12, 1100, &adc_chars);
  raw  = analogRead(battPin);
  volt = raw / vScale1;
  Serial.print ("Battery = ");
  Serial.println (volt);
  Serial.print ("Raw = ");
  Serial.println (raw);

  SPI.begin(EPD_SCLK, EPD_MISO, EPD_MOSI);
  // Display start
  display.init();
  display.setRotation(3);

  display.fillRect(0, 0, 250, 120, GxEPD_WHITE);
  display.setFont(&FreeSansBold12pt7b);
  display.setTextColor(GxEPD_BLACK);
  display.setCursor(40, 50);
  display.println("Komoot Nav"); //Boot Image
  display.setFont(&FreeSansBold9pt7b);
  display.setCursor(75, 70);
  display.println(connectText);
  display.updateWindow(0, 0, 220, 120, true);

  // Battery voltage
  pinMode(battPin, INPUT);
  Battery_check();

  display.update();

  BLEDevice::init("komootnav");
  // code added for pairing
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);
  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
                                         CHARACTERISTIC_UUID,
                                         BLECharacteristic::PROPERTY_READ |
                                         BLECharacteristic::PROPERTY_WRITE
                                       );

  pCharacteristic->setValue("Hello World says Neil");
  pService->start();
  // BLEAdvertising *pAdvertising = pServer->getAdvertising();  // this still is working for backward compatibility
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("Characteristic defined! Now you can read it in your phone!");
  //end code added for pairing

  // Retrieve a Scanner and set the callback we want to use to be informed when we
  // have detected a new device.  Specify that we want active scanning and start the
  // scan to run for 5 seconds.
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(20, false);

} // End of setup.



void loop() {

  // If the flag "doConnect" is true then we have scanned for and found the desired
  // BLE Server with which we wish to connect.  Now we connect to it.  Once we are
  // connected we set the connected flag to be true.
  if (doConnect == true) {
    if (connectToServer()) {
      Serial.println("We are now connected to the BLE Server.");
      display.updateWindow(0, 0, 220, 120, false);
      display.fillRect(0, 0, 220, 120, GxEPD_WHITE);
      display.updateWindow(0, 0, 220, 120, true);

      Battery_check();
    } else {
      //display.update();
      Serial.println("We have failed to connect to the server; there is nothing more we will do.");
    }
    doConnect = false;
    display.update();
  }


  unsigned long currentMillis = millis();


  if (currentMillis - previousMillis > interval) {
    previousMillis = currentMillis;
    Battery_check();
  }

  // If we are connected to a peer BLE Server, update the characteristic each time we are reached
  // with the current time since boot.
  if (connected) {

    std::string value = pRemoteCharacteristic->readValue();//this crashes sometimes, receives the whole data packet
    if (value.length() > 4) {
      //in case we have update flag but characteristic changed due to navigation stop between
      updated = false;

      std::string distance;
      distance = value.substr(5, 8);
      uint32_t dist = distance[0] | distance[1] << 8 | distance[2] << 16 | distance[3] << 24;
      if (dist2 != dist) {
        dist2 = dist;
        showPartialUpdate_dist(dist2);  //<------------- DISTANCE

        Serial.print("Distance update: ");
        Serial.println(dist2);
        updated = true;
      } //display distance in metres

      street = value.substr(9);//this causes abort when there are not at least 9 bytes available
      if (street != old_street) {
        old_street = street;
        old_firstWord = firstWord;
        firstWord = street.substr(0, street.find(", "));
        showPartialUpdate_street(firstWord, old_firstWord); //<------- STREET
        Serial.print("Street update: ");
        Serial.println(firstWord.c_str());
        updated = true;
      } //extracts the firstword of the street name and displays it

      std::string direction;
      direction = value.substr(4, 4);
      uint8_t d = direction[0];
      if (d != dir) {
        dir = d;
        showPartialUpdate_dir(dir);  // <-------------------------DIRECTION
        Serial.print("Direction update: ");
        Serial.println(d);
        updated = true;
      } //display direction

      /* if (updated_dir == 1  ) {
          display.updateWindow(0, 0, 64, 64, true);
         //       Serial.println("Partial update");
         updated = false;
        }
        if (updated_dist == 1) {
        display.updateWindow(105, 33, 144, 25, true);
        updated_dist = 0;
        }
        if (updated_dir == 1) {
        display.updateWindow(0, 0, 65, 65, true);
        updated_dir = 0;
        }
        if (updated_street == 1) {
        display.updateWindow(10, 66, 239, 54, true);
        updated_street = 0;
        //display.update();
        }*/


      if (dist2 > 100) {
        esp_sleep_enable_timer_wakeup(4000000);
        delay(4000);
      } else {
        delay(600); // Delay between loops.
      }
    } else if (doScan) {
      BLEDevice::getScan()->start(0);  // this is just eample to start scan after disconnect, most likely there is better way to do it in arduino
    }
  }
} // End of loop
