#include <Arduino.h>

#include <NimBLEDevice.h>

/*
  Reverse engineered uuids and data formats of the F7 oximeter device

  - Custom service SVC of device DEV has 2 characteristics
  - Sending value VAL to characteristic REQ triggers a notification NFY
  - Notification characteristic NFY receives values of type f7_data_t

  Limitations (I just don't know how: the Sapiential SpO2 android app can, Wearfit BO iOS apps can't)

  - The F7 device only advertises at startup
  - The F7 device only responds to one connection attempt
  - The F7 device does not allow reconnect if it was out of range
*/

const char DEV[] = "6E40F431-B5A3-F393-E0A9-E50E24DCCA9E";
const char SVC[] = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
const char NFY[] = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";
const char REQ[] = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";
const char VAL[] = {0xab, 0x00, 0x03, 0xff, 0x30, 0x80};

typedef struct {
  uint8_t head[6];
  uint8_t ppm;
  uint8_t spO2;
  uint8_t deziPI;
} f7_data_t;

const uint32_t POLL_INTERVAL_MS = 1000;
const uint32_t CONN_TIMEOUT_S = 5;
const uint32_t SCAN_DURATION_S = 5;

// state machine: scan <-> connect -> poll -> scan
bool doConnect = false; // set to true if we find our service
bool doPoll = false;    // set to true if we connected to our service

NimBLEAddress devAddress;

// Notification callback
void notifyCB(NimBLERemoteCharacteristic *pRemoteCharacteristic, uint8_t *pData,
              size_t length, bool isNotify) {
  
  f7_data_t *pDataF7 = (f7_data_t *)pData;
  
  Serial.print("Notification:");
  while (length--) {
    Serial.printf(" %02x", *(pData++));
  }
  Serial.println();

  if (pDataF7->ppm && pDataF7->spO2 && pDataF7->deziPI) {
    NimBLEClient *pClient = pRemoteCharacteristic->getRemoteService()->getClient();
    Serial.printf("%s(%d) - SpO2: %u, Perfusionsindex: %u.%u, Puls: %u\n",
      pClient->getPeerAddress().toString().c_str(), pClient->getRssi(),
      pDataF7->spO2, pDataF7->deziPI / 10, pDataF7->deziPI % 10, pDataF7->ppm);
  }
}

void scanEndedCB(NimBLEScanResults results) 
{ 
  Serial.println("Scan ended");
  doConnect = true;
}

class ClientCallbacks : public NimBLEClientCallbacks {
  void onDisconnect(NimBLEClient *pClient) {
    Serial.printf("Disconnected %s - starting new scan\n",
                  pClient->getPeerAddress().toString().c_str());
    doPoll = false;
    NimBLEDevice::deleteClient(pClient);
    NimBLEDevice::getScan()->start(SCAN_DURATION_S, scanEndedCB);
  };
};

static ClientCallbacks clientCB;

// Handle advertisment callbacks of scans */
class AdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice *advertisedDevice) {
    if (advertisedDevice->isAdvertisingService(NimBLEUUID(DEV))) {
      Serial.print("Found our service - ");
      Serial.println(advertisedDevice->toString().c_str());
      // Save the device in a global for the connect
      devAddress = advertisedDevice->getAddress();
      NimBLEDevice::getScan()->stop();
    }
  };
};

bool subscribeToNotification(NimBLEClient *pClient) {
  NimBLERemoteService *pSvc = pClient->getService(SVC);
  if (pSvc) {
    NimBLERemoteCharacteristic *pChr = pSvc->getCharacteristic(NFY);
    if (pChr) {
      Serial.printf("Characteristic handle is x%04x\n", pChr->getHandle());
      if (pChr->subscribe(true, notifyCB, true)) {
        Serial.println("Characteristic notification subscribed");
        return true;
      } else {
        Serial.println("Characteristic notification failed");
      }
    } else {
      Serial.println("Characteristic not found");
    }
  } else {
    Serial.println("Service not found");
  }

  return false;
}

// Connects to the F7 oximeter found by advertisements
NimBLEClient *connectToServer() {
  Serial.printf("Connect to %s\n", devAddress.toString().c_str());
  NimBLEClient *pClient = NimBLEDevice::createClient();
  if( pClient ) {
    pClient->setClientCallbacks(&clientCB, false);
    // Set initial connection parameters
    // These settings are safe for 3 clients to connect reliably. 
    // Timeout must be >= 100ms and should be a multiple of the interval
    // Min interval: 12 * 1.25ms = 15, Max interval: 12 * 1.25ms = 15,
    // Latency = 0, 60 * 10ms = 600ms timeout
    pClient->setConnectionParams(12, 12, 0, 60);
    pClient->setConnectTimeout(CONN_TIMEOUT_S);

    if (pClient->connect(devAddress)) {
      Serial.printf("Connected with RSSI %d\n", pClient->getRssi());

      if( subscribeToNotification(pClient) ) {
        return pClient;
      }
      else {
        pClient->disconnect();
      }
    } else {
      Serial.println("Connection failed");
    }
    NimBLEDevice::deleteClient(pClient);
  }
  else {
    Serial.println("Create client failed");
  }

  return nullptr;
}

bool pollService( NimBLEClient *pClient ) {
  NimBLERemoteService *pSvc = nullptr;
  NimBLERemoteCharacteristic *pChr = nullptr;
  pSvc = pClient->getService(SVC);
  if (pSvc) {
    pChr = pSvc->getCharacteristic(REQ);
    if (pChr) {
      Serial.printf("Characteristic handle x%04x", pChr->getHandle());
      if (pChr->writeValue(VAL)) {
        Serial.println(" requested");
        return true;
      } else {
        Serial.println(" request failed");
      }
    } else {
      Serial.println("Characteristic gone");
    }
  } else {
    Serial.println("Service gone");
  }
  pClient->disconnect();
  return false;
}

void setup() {
  Serial.begin(115200);
  Serial.println("\nOximeter F7 BLE client");
  Serial.println("compiled " __DATE__ " " __TIME__);

  NimBLEDevice::init("");  // We dont advertise -> no device name
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);  // +9db (default is 3db)
  NimBLEScan *pScan = NimBLEDevice::getScan();
  pScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());
  pScan->setInterval(45);  // scan interval in ms
  pScan->setWindow(15);  // scan duration in ms
  pScan->setActiveScan(true);

  Serial.printf("Starting scan for %s\n", DEV);
  pScan->start(0, scanEndedCB);  // start scan
}

void loop() {
  static uint32_t lastPoll = 0;
  static NimBLEClient *pClient = nullptr;

  if( doPoll ) {
    if( millis() - lastPoll > POLL_INTERVAL_MS ) {
      lastPoll = millis();
      doPoll = pollService(pClient);
    }
  } else if( doConnect ) {
    doConnect = false;
    pClient = connectToServer();
    if( pClient ) {
      lastPoll = millis() - POLL_INTERVAL_MS;
      doPoll = true;
    } else {
      Serial.println("Failed to connect - start new scan");
      NimBLEDevice::getScan()->start(SCAN_DURATION_S, scanEndedCB);
    }
  }

  delay(1);
}