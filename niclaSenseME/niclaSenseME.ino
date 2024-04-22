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


// time
//#include <TimeLib.h>




// defining filesystem object
constexpr auto userRoot{ "storedSensorData" };  // Name of filesystem root
mbed::BlockDevice* spif;
mbed::LittleFileSystem fs{ userRoot };


// sensor objects to be used for retreiving sensor data
SensorXYZ accelLinear(SENSOR_ID_LACC);  // negative x is our upwards
Sensor barometer(SENSOR_ID_BARO);
SensorBSEC bsec(SENSOR_ID_BSEC);

// acceleration limit for decision between movement and stop of elevator
const int updateIntervall = 100;  //ms
const int numberAccelerationValuesOverLimit = 3;
const int longestDriveDuration = 30000;       //ms
const unsigned int detectionDuration = 1000;  //ms
const int accelerationLimit = 100;
const int accelerationLimitLow = 60;
unsigned int i = 0;


// csv head line
const String headLine = "timestamp;starttime;endtime;pressureStart;pressureEnd;accelMax;accelMin;accelEnd;brakeStart;level;co2_eq;b_voc_eq;accuracy;\r\n";
const String test_headLine = "timestamp;accelerationX*(-1);pressure\r\n";


// variables for time measurement
long updateTime = 0;
long transferMaxDurationMillis = 30000;
long lastConnectionMillis;
String dateAndTime = "";

// helper variables for storing/retreiving file contents
const int bytesPerLineForStorage = 128;
const int bytesPerLine = 128;
String fileline = "";
const String eof_indicator = "ende";


// helper variable for storing measurement data before processing
//int accelerationValues[(1000 / updateIntervall) * (longestDriveDuration / 1000)];


// BLE objects
BLEService serviceFileTransmission("0008");
BLEStringCharacteristic characteristicFileTransmission("0007",  // standard 16-bit characteristic UUID
                                                       BLERead | BLEWrite | BLENotify,
                                                       bytesPerLine);




void setup() {
  Serial.begin(115200);

  // wait for serial startup for max. 5s
  for (const auto timeout = millis() + 5000; !Serial && millis() < timeout; delay(500))
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

  nicla::leds.setColor(red);

  // stop data storage before flashstorage is full (~2h)
  if (millis() < 7200000) {

    // Store data from sensors to the SPI Flash Memory after specified Intervall
    if (millis() - updateTime >= updateIntervall) {
      updateTime = millis();
      // read actual sensor data
      BHY2.update();

      // Alternative Erkennung
      // if (i < numberAccelerationValuesOverLimit) {
      //   if (abs(int(accelLinear.x())) > accelerationLimit) {
      //     i++;
      //   } else if ((updateTime - detection_startTime) > detectionDuration) {
      //     i = 0;
      //   }
      ////////////////////////
      if (i < numberAccelerationValuesOverLimit) {
        if (abs(int(accelLinear.x())) > accelerationLimit) {
          i++;
        } else if (i < 2) {
          i = 0;
        } else {
          i--;
        }
      } else {
        measureElevatorRun();
        i = 0;
      }

      test_storeData();
    }
  } else {
    nicla::leds.setColor(yellow);
  }
}




bool measureElevatorRun() {

  nicla::leds.setColor(yellow);

  unsigned int accel_start = millis();
  float pressure_start = barometer.value();


  //unsigned int i = 0;
  unsigned int j = 0;
  unsigned int k = 0;
  unsigned int l = 0;

  // define direction of elevator run
  bool upwards;
  int accel = -1 * int(accelLinear.x());  // x axis on sensorboard is inverted/flipped
  if (accel > 0) {
    upwards = true;
  } else {
    upwards = false;
  }
  int accel_max = 0;
  int accel_min = 0;
  unsigned int accel_endTime = 0;
  unsigned int brake_startTime = 0;

  while (true) {

    if (millis() - updateTime >= updateIntervall) {

      BHY2.update();
      test_storeData();
      accel = -1 * int(accelLinear.x());  // x axis on sensorboard is inverted/flipped
      if (accel > accel_max) {
        accel_max = accel;
      }
      if (accel < accel_min) {
        accel_min = accel;
      }
      //accelerationValues[i] = int(accelLinear.x());
      //i++;


      // if elevator has not begun braking...
      if (j < numberAccelerationValuesOverLimit) {

        if (accel_endTime == 0) {
          // check end of acceleration period
          if (l == numberAccelerationValuesOverLimit) {
            // einmaliges Setzen der Variable, danach ist l immer größer
            accel_endTime = millis();
            nicla::leds.setColor(blue);
            l = l + 1;
          } else if (abs(accel) < accelerationLimitLow) {
            l++;
          } else if (l < 2) {
            l = 0;
          } else {
            l--;
          }
        }

        // check end of elevator run by detecting braking
        if (upwards) {
          if (accel < (accelerationLimit * -1)) {
            j++;
          } else if (j < 2) {
            j = 0;
          } else {
            j--;
          }
        } else {
          if (accel > accelerationLimit) {
            j++;
          } else if (j < 2) {
            j = 0;
          } else {
            j--;
          }
        }

      } else {
        nicla::leds.setColor(green);

        if (brake_startTime == 0) {
          brake_startTime = millis();
        }
        // check end of elevator braking by checking if acceleration->0
        if (k < numberAccelerationValuesOverLimit) {
          if (abs(accel) < accelerationLimitLow) {
            k++;
          } else if (k < 2) {
            k = 0;
          } else {
            k--;
          }
        } else {
          // leave while loop
          break;
        }
      }
    }
  }

  float pressure_end = barometer.value();
  unsigned int brake_endTime = millis();

  nicla::leds.setColor(red);
  if (calculateRun(
        /*accel_start*/ accel_start, /*brake_endTime*/ brake_endTime,
        /*pressure_start*/ pressure_start, /*pressure_end*/ pressure_end,
        /*accel_max*/ accel_max, /*accel_min*/ accel_min,
        /*accel_endTime*/ accel_endTime, /*brake_startTime*/ brake_startTime)) {
    return true;
  } else {
    return false;
  }
}

bool calculateRun(int accel_start, unsigned int brake_endTime, float pressure_start, float pressure_end, int accel_max, int accel_min, unsigned int accel_endTime, unsigned int brake_startTime) {

  int level = 10;

  if (pressure_start > 0) {
    storeData(/*accel_start*/ accel_start, /*brake_endTime*/ brake_endTime,
              /*pressureStart*/ pressure_start, /*pressureEnd*/ pressure_end,
              /*accel_max*/ accel_max, /*accel_min*/ accel_min,
              /*accel_endTime*/ accel_endTime, /*brake_startTime*/ brake_startTime,
              /*level*/ level);
    return true;
  } else {
    return false;
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
  BLE.setLocalName("Nicla für Studienarbeit");
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
  accelLinear.begin();
  barometer.begin();
  bsec.begin();

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

              //   int hr = hour.toInt();
              //   int min = minute.toInt();
              //   int sec = 0;
              //   int d = day.toInt();
              //   int m = month.toInt();
              //   int y = year.toInt();
              //   setTime(hr, min, sec, d, m, y);
              //   Serial.print("arduino time.now: ");
              //   Serial.println(hour() + minute() + second() + ", " + month() + year());

              Serial.print("dateAndTime: ");
              Serial.println(dateAndTime);

              Serial.print("dateAndTime split up: ");
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
      nicla::leds.setColor(blue);
      return false;
    }
  }
  Serial.println("No more files to transfer.");
  return true;
}




void storeData(int accel_start, unsigned int brake_endTime, float pressure_start, float pressure_end, int accel_max, int accel_min, unsigned int accel_endTime, unsigned int brake_startTime, int level) {
  // name of file on LittleFS filesystem to append data - filename tbd
  constexpr auto filename{ "elevatorRuns.csv" };

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


  // Save sensors data as a CSV line
  auto csvLine = dataToCsvLine(
    /*accel_start*/ accel_start, /*brake_endTime*/ brake_endTime,
    /*pressure_start*/ pressure_start, /*pressure_end*/ pressure_end,
    /*accel_max*/ accel_max, /*accel_min*/ accel_min,
    /*accel_endTime*/ accel_endTime, /*brake_startTime*/ brake_startTime,
    /*level*/ level);

  auto ret = file.write(csvLine.c_str(), csvLine.length());
  if (ret != csvLine.length()) {
    Serial.print("Error writing data: ");
    Serial.println(ret);
  }
  // close file after writing data as one line
  file.close();
}

String dataToCsvLine(int accel_start, unsigned int brake_endTime, float pressure_start, float pressure_end, int accel_max, int accel_min, unsigned int accel_endTime, unsigned int brake_startTime, int level) {
  String line = "";

  // Pre-allocate maxLine bytes for line -> amount tbd
  constexpr size_t maxLine{ bytesPerLineForStorage };
  line.reserve(maxLine);


  // create line with relevant sensor data
  line += millis();
  line += ";";
  line += accel_start;
  line += ";";
  line += brake_endTime;
  line += ";";
  line += pressure_start;
  line += ";";
  line += pressure_end;
  line += ";";
  line += accel_max;
  line += ";";
  line += accel_min;
  line += ";";
  line += accel_endTime;
  line += ";";
  line += brake_startTime;
  line += ";";
  line += level;
  //   for (int i = 0; i < sizeof(accelerationValues); i++) {
  //     line += accelerationValues[i];
  //     line += "-";
  //     accelerationValues[i] = 0;
  //   }
  line += bsec.co2_eq();
  line += ";";
  line += bsec.b_voc_eq();
  line += ";";
  line += bsec.accuracy();
  line += ";";
  //   line += bsec.comp_h();
  //   line += ";";
  //   line += bsec.comp_t();
  line += "\r\n";

  Serial.print("line of sensor values for next elevator run: ");
  Serial.println(line);

  return line;
}




void test_storeData() {
  // name of file on LittleFS filesystem to append data - filename tbd
  constexpr auto filename{ "continuous_data.csv" };

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
      auto title_error = file.write(test_headLine.c_str(), test_headLine.length());

      if (title_error != headLine.length()) {
        Serial.print("Error writing csv head line: ");
        Serial.println(title_error);
      }
    }
  }


  // Save sensors data as a CSV line
  auto csvLine = test_dataToCsvLine();

  auto ret = file.write(csvLine.c_str(), csvLine.length());
  if (ret != csvLine.length()) {
    Serial.print("Error writing data: ");
    Serial.println(ret);
  }
  // close file after writing data as one line
  file.close();
}

String test_dataToCsvLine() {
  String line = "";

  // Pre-allocate maxLine bytes for line -> amount tbd
  constexpr size_t maxLine{ bytesPerLineForStorage };
  line.reserve(maxLine);


  // create line with relevant sensor data
  line += millis();
  line += ";";
  line += (int(accelLinear.x()) * (-1));
  line += ";";
  line += barometer.value();
  line += "\r\n";

  Serial.print("next line of sensor values: ");
  Serial.println(line);

  return line;
}