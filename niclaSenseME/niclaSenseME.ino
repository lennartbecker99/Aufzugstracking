// nicla features
#include <Nicla_System.h>  // https://docs.arduino.cc/tutorials/nicla-sense-me/cheat-sheet#rgb-led
#include <Arduino_BHY2.h>  //https://docs.arduino.cc/tutorials/nicla-sense-me/cheat-sheet#standalone-mode

// storing to & reading from Flash Storage
// https://github.com/arduino/nicla-sense-me-fw/tree/main/Arduino_BHY2/examples/StandaloneFlashStorage
#include <BlockDevice.h>
#include <Dir.h>
#include <File.h>
#include <FileSystem.h>
#include <LittleFileSystem.h>

// communication via BLE
#include <ArduinoBLE.h>




// defining filesystem object
constexpr auto userRoot{ "storedSensorData" };  // Name of filesystem root
mbed::BlockDevice* spif;
mbed::LittleFileSystem fs{ userRoot };


// sensor objects to be used for retreiving sensor data
SensorXYZ accel(SENSOR_ID_ACC);

// csv head line
const String headLine = "timestamp;accelX;accelY;accelZ\r\n";

// intervalls for storing/printing file contents
const int storeIntervall = 10000;
const int printIntervall = 20000;

// variables for time measurement
long transferMaxDurationMillis = 30000;
long lastConnectionMillis;
String dateAndTime = "";

// helper variables for storing/retreiving file contents
const int bytesPerLine = 128;
String fileline = "";
const String eof_indicator = "ende";

// BLE objects
BLEService serviceFileTransmission("0008");
BLEStringCharacteristic characteristicFileTransmission("0007",  // standard 16-bit characteristic UUID
                                                       BLERead | BLEWrite | BLENotify,
                                                       bytesPerLine);




void setup() {
  Serial.begin(115200);

  // wait for serial startup for max. 2,5s
  for (const auto timeout = millis() + 2500; !Serial && millis() < timeout; delay(250))
    ;

  // initiate nicla features
  nicla::begin();
  nicla::leds.begin();  //https://docs.arduino.cc/tutorials/nicla-sense-me/cheat-sheet#rgb-led

  while (!initiateFS())
    ;

  if (fileTransfer()) {
    Serial.println("Files transferred to client.");
    Serial.println("Return to normal mode.");
  } else {
    Serial.print("No client connection requested for ");
    Serial.print(transferMaxDurationMillis);
    Serial.println("ms.");
    Serial.println("Begin with normal mode.");
  }

  while (!initiateSensors())
    ;
}




void loop() {
  static auto printTime = millis();
  static auto storeTime = millis();
  static auto statsTime = millis();

  nicla::leds.setColor(red);

  // read actual sensor data
  BHY2.update();

  // Store data from sensors to the SPI Flash Memory after specified Intervall
  if (millis() - storeTime >= storeIntervall) {
    storeTime = millis();
    storeData();
  }

  // List stats + files on the Flash Memory & print contents after specified Intervall
  if (millis() - printTime >= printIntervall) {
    printTime = millis();
    printStats();
    listDirsContents();
  }
}




bool initiateBLE() {
  // begin BLE initialization
  Serial.print("Starting BLE...");

  if (!BLE.begin()) {
    Serial.println(" failed!");
    return false;
  }
  Serial.print(" BLE running...");

  // set BLE device information & advertised data
  BLE.setLocalName("Nicla von Flo");
  BLE.setAdvertisedService(serviceFileTransmission);                          // add service UUID to advertising
  serviceFileTransmission.addCharacteristic(characteristicFileTransmission);  // add characteristic to service
  BLE.addService(serviceFileTransmission);                                    // add  service to device

  // start advertising
  BLE.advertise();

  //turn LED to blue for indicating BLE readiness
  nicla::leds.setColor(blue);
  Serial.println("Bluetooth® device active, waiting for connections...");

  return true;
}

bool initiateFS() {
  Serial.print("Loading the SPI Flash Storage and LittleFS filesystem...");

  // init core-wide instance of SPIF Block Device
  spif = mbed::BlockDevice::get_default_instance();
  spif->init();

  // initiate filesystem
  int err = fs.mount(spif);
  if (err) {
    err = fs.reformat(spif);
    Serial.print("Error mounting file system: ");
    Serial.println(err);
    return false;
  }
  Serial.println(" filesystem mounted to flash storage.");

  return true;
}

bool initiateSensors() {
  Serial.print("Initialising the sensors... ");

  BHY2.begin();
  accel.begin();

  Serial.println(" initialised sensors.");

  return true;
}




bool fileTransfer() {

  bool transferSuccess = false;
  // Open the root of the filesystem
  mbed::Dir dir(&fs, "/");
  dirent ent;
  mbed::File file;

  while (!initiateBLE())
    ;

  Serial.println(serviceFileTransmission.uuid());
  Serial.println(characteristicFileTransmission.uuid());

  // Cycle through all the directory entries
  while ((dir.read(&ent)) > 0) {
    if (ent.d_type == DT_REG) {

      // open the file in read-only mode
      auto ret = file.open(&fs, ent.d_name);  // returns <0 if failure
      if (ret) {
        Serial.print("Unable to open file ");
        Serial.println(ent.d_name);
        break;
      }

      if (file.size() < 1) {
        file.close();
        fs.remove(ent.d_name);
        break;
      }

      size_t totalLen{ file.size() };
      characteristicFileTransmission.writeValue(ent.d_name);

      nicla::leds.setColor(blue);
      transferSuccess = false;
      lastConnectionMillis = millis();

      while (millis() - lastConnectionMillis < transferMaxDurationMillis) {

        // wait for BLE central device
        BLEDevice central = BLE.central();

        // if a central is connected
        if (central) {
          Serial.print("Connected to central: ");
          // print the central's BT address
          Serial.println(central.address());
          // turn LED to green for indicating connection
          nicla::leds.setColor(green);
          lastConnectionMillis = millis();

          long now = millis();
          while (millis() - now < 2000)
            ;
          long lastWrite = millis();

          while (central.connected()) {

            if (transferSuccess && characteristicFileTransmission.written()) {

              // Empty file after reading all the content.
              Serial.print("file to be removed: ");
              Serial.println(ent.d_name);
              file.close();

              ret = file.open(&fs, ent.d_name, O_TRUNC);
              if (ret < 0) {
                Serial.println("Unable to truncate file");
              } else {
                // delete empty file from Flash Storage
                file.close();
                fs.remove(ent.d_name);
                Serial.println("file removed.");
              }

              break;
            }

            if (dateAndTime == "" && characteristicFileTransmission.written()) {
              dateAndTime = characteristicFileTransmission.value();
              String day = "";
              for (int i = 5; i <= 6; i++) {
                day += dateAndTime[i];
              }
              String month = "";
              for (int i = 0; i <= 1; i++) {
                month += dateAndTime[i];
              }
              String year = "";
              for (int i = 10; i <= 13; i++) {
                year += dateAndTime[i];
              }
              String hour = "";
              for (int i = 15; i <= 16; i++) {
                hour += dateAndTime[i];
              }
              String minute = "";
              for (int i = 19; i <= 20; i++) {
                minute += dateAndTime[i];
              }

              Serial.println(dateAndTime);
              Serial.print(day);
              Serial.print(month);
              Serial.print(year);
              Serial.print(hour);
              Serial.println(minute);
            }

            if (totalLen > 0) {
              if (dateAndTime != "" && characteristicFileTransmission.written()) {
                char buf[bytesPerLine]{};

                auto read = file.read(buf, bytesPerLine);
                totalLen -= read;
                for (const auto& c : buf) {
                  fileline.concat(c);
                }
                characteristicFileTransmission.writeValue(fileline);
                lastWrite = millis();
                Serial.println(fileline);
                fileline = "";
              } else {
                if (millis() - lastWrite > transferMaxDurationMillis) return false;
              }

            } else {
              if (!transferSuccess) {
                characteristicFileTransmission.writeValue(eof_indicator);
                transferSuccess = true;
              }
            }
          }
        }
        if (transferSuccess) break;
      }
      if (transferSuccess) continue;
      return false;
    }
  }
  Serial.println("No more files to transfer.");
  return true;
}




void storeData() {
  // name of file on LittleFS filesystem to append data - filename tbd
  constexpr auto filename{ "sensors.csv" };

  // Open in file in write mode.
  // Create if doesn't exists, otherwise open in append mode.
  mbed::File file;

  auto append_error = file.open(&fs, filename, O_WRONLY | O_APPEND);

  if (append_error) {
    Serial.println("File does not exist yet");

    auto create_error = file.open(&fs, filename, O_WRONLY | O_CREAT | O_APPEND);
    if (create_error) {
      Serial.println("File cannot be created.");
      return;

    } else {
      auto title_error = file.write(headLine.c_str(), headLine.length());

      if (title_error != headLine.length()) {
        Serial.print("Error writing csv head line: ");
        Serial.println(title_error);
      }
    }
  }

//   auto err = file.open(&fs, filename, O_WRONLY | O_CREAT | O_APPEND);
//   if (err) {
//     Serial.print("Error opening file for writing: ");
//     Serial.println(err);
//     return;
//   }


  // Save sensors data as a CSV line
  auto csvLine = sensorsToCSVLine();

  auto ret = file.write(csvLine.c_str(), csvLine.length());
  if (ret != csvLine.length()) {
    Serial.print("Error writing data: ");
    Serial.println(ret);
  }
  // close file after writing data as one line
  file.close();
}

String sensorsToCSVLine() {
  String line = "";

  // Pre-allocate maxLine bytes for line -> amount tbd
  constexpr size_t maxLine{ bytesPerLine };
  line.reserve(maxLine);

  // create line with relevant sensor data
  line += millis();
  line += ";";
  line += accel.x();
  line += ";";
  line += accel.y();
  line += ";";
  line += accel.z();
  line += "\r\n";

  Serial.print(line.length());
  Serial.print(": ");
  Serial.println(line);

  return line;
}




void printStats() {
  // retreive stats from file system object
  struct statvfs stats {};
  fs.statvfs(userRoot, &stats);

  auto blockSize = stats.f_bsize;

  Serial.print("Total Space [Bytes]:  ");
  Serial.println(stats.f_blocks * blockSize);  // calculate space from block size & number of blocks
  Serial.print("Free Space [Bytes]:   ");
  Serial.println(stats.f_bfree * blockSize);
  Serial.print("Used Space [Bytes]:   ");
  Serial.println((stats.f_blocks - stats.f_bfree) * blockSize);
  Serial.println();
}




void listDirsContents() {
  // create path to root of filesystem
  String baseDirName = "/";
  baseDirName += userRoot;

  Serial.print("Listing files on ");
  Serial.print(baseDirName);
  Serial.println(" Filesystem");

  // Open root of filesystem
  mbed::Dir dir(&fs, "/");
  dirent ent;

  // Cycle through all directory entries
  while ((dir.read(&ent)) > 0) {
    // action according to type of directory entry
    switch (ent.d_type) {
      case DT_DIR:  // subdirectory
        {
          Serial.print("Directory ");
          Serial.println(ent.d_name);
          break;
        }
      case DT_REG:  // file
        {
          Serial.print("Regular File ");
          Serial.print(ent.d_name);

          // Declare and open the file in read-only mode
          mbed::File file;
          auto ret = file.open(&fs, ent.d_name);
          // notify if file cannot be opened & jump to end of loop
          if (ret) {
            Serial.println("Unable to open file");
            continue;
          }
          Serial.print(" [");
          Serial.print(file.size());
          Serial.println(" bytes]");

          // check file contents
          if (file.size() > 0) {
            printFile(file);  // replace by function for sending contents via BLE

            // Empty file after reading all the content.
            file.close();
            /*ret = file.open(&fs, ent.d_name, O_TRUNC);
                // check success of erasing file contents
                if (ret < 0)
                    Serial.println("Unable to truncate file");
            } else {
                // Remove file if empty.
                file.close();
                fs.remove(ent.d_name);*/
          }

          break;
        }
      default:
        {
          Serial.print("Other ");
          break;
        }
    }
  }
}

void printFile(mbed::File& file) {
  // Read and print file len-bytes at time
  // to preserve RAM
  constexpr size_t len{ bytesPerLine };

  size_t totalLen{ file.size() };

  while (totalLen > 0) {
    char buf[len]{};

    auto read = file.read(buf, len);
    totalLen -= read;
    for (const auto& c : buf)
      Serial.print(c);
  }
  Serial.println();
}