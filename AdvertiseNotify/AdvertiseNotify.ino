/*
  Battery Monitor

  This example creates a Bluetooth® Low Energy peripheral with the standard battery service and
  level characteristic. The A0 pin is used to calculate the battery level.

  The circuit:
  - Arduino MKR WiFi 1010, Arduino Uno WiFi Rev2 board, Arduino Nano 33 IoT,
    Arduino Nano 33 BLE, or Arduino Nano 33 BLE Sense board.

  You can use a generic Bluetooth® Low Energy central app, like LightBlue (iOS and Android) or
  nRF Connect (Android), to interact with the services and characteristics
  created in this sketch.

  This example code is in the public domain.
*/

#include <ArduinoBLE.h>
#include <Nicla_System.h>

// Bluetooth® Low Energy Battery Service
BLEService myService("0008");

// Bluetooth® Low Energy Battery Level Characteristic
BLEStringCharacteristic myCharacteristic("0007",  // standard 16-bit characteristic UUID
                                         BLERead | BLEWrite | BLENotify,
                                         50);

long previousMillis = 0;
String val = "";
bool checker = false;
int counter = 1;

void setup() {
  Serial.begin(115200);  // initialize serial communication
  while (!Serial)
    ;

  pinMode(LED_BUILTIN, OUTPUT);  // initialize the built-in LED pin to indicate when a central is connected

  // begin initialization
  if (!BLE.begin()) {
    Serial.println("starting BLE failed!");

    while (1)
      ;
  }

  /* Set a local name for the Bluetooth® Low Energy device
     This name will appear in advertising packets
     and can be used by remote devices to identify this Bluetooth® Low Energy device
     The name can be changed but maybe be truncated based on space left in advertisement packet
  */
  BLE.setLocalName("Nicla von Flo");
  BLE.setAdvertisedService(myService);            // add the service UUID
  myService.addCharacteristic(myCharacteristic);  // add the battery level characteristic
  BLE.addService(myService);                      // Add the battery service
  myCharacteristic.writeValue("testval");         // set initial value for this characteristic

  /* Start advertising Bluetooth® Low Energy.  It will start continuously transmitting Bluetooth® Low Energy
     advertising packets and will be visible to remote Bluetooth® Low Energy central devices
     until it receives a new connection */

  // start advertising
  BLE.advertise();

  Serial.println("Bluetooth® device active, waiting for connections...");
}

void loop() {
  // wait for a Bluetooth® Low Energy central
  BLEDevice central = BLE.central();

  // if a central is connected to the peripheral:
  if (central) {
    Serial.print("Connected to central: ");
    // print the central's BT address:
    Serial.println(central.address());
    // turn on the LED to indicate the connection:
    digitalWrite(LED_BUILTIN, HIGH);

    // check the battery level every 200ms
    // while the central is connected:
    while (central.connected()) {
      if (myCharacteristic.written()) {
        Serial.print("written:");
        val = myCharacteristic.value();
        Serial.println(val);
        if (counter > 10) {
          myCharacteristic.writeValue("ende");
        } else {
            counter = counter + 1;
          if (checker) {
            myCharacteristic.writeValue("abc");
            checker = false;
          } else {
            myCharacteristic.writeValue("def");
            checker = true;
          }
        }
      }
      //kombinieren mit Zeitabfrage für Energieeinsparung
      //   long currentMillis = millis();
      //   if (currentMillis - previousMillis >= 1000) {
      //     myCharacteristic.writeValue("neuer Wert");
      //     previousMillis = currentMillis;
      //   }
    }
    // when the central disconnects, turn off the LED:
    digitalWrite(LED_BUILTIN, LOW);
    Serial.print("Disconnected from central: ");
    Serial.println(central.address());
    counter = 1;
  } else {
    myCharacteristic.writeValue("erster Wert");
  }
}
