// storing to & reading from Flash Storage
#include <BlockDevice.h>
#include <Dir.h>
#include <File.h>
#include <FileSystem.h>
#include <LittleFileSystem.h>
#include <Arduino_BHY2.h>




// defining filesystem object
constexpr auto userRoot{ "storedSensorData" };  // Name of filesystem root
mbed::BlockDevice* spif;
mbed::LittleFileSystem fs{ userRoot };

// sensor objects to be used for retreiving sensor data
Sensor temp(SENSOR_ID_TEMP);
SensorXYZ accel(SENSOR_ID_ACC);

// intervalls for storing/printing file contents
const int storeIntervall = 1000;
const int printIntervall = 5000;

// bytes per line in storage file
const int bytesPerLine = 128;

void setup() {
  Serial.begin(115200);

  // wait for serial startup for max. 2,5s
  for (const auto timeout = millis() + 2500; !Serial && millis() < timeout; delay(250))
    ;

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
    while (true)  // do nothing if filesystem was not mounted correctly
      ;
  }
  Serial.println(" filesystem mounted ro flash storage.");

  Serial.print("Initialising the sensors... ");
  BHY2.begin();

  temp.begin();
  accel.begin();
  Serial.println(" initialised sensors.");
}




void loop() {
  static auto printTime = millis();
  static auto storeTime = millis();
  static auto statsTime = millis();

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




void storeData() {
  // name of file on LittleFS filesystem to append data - filename tbd
  constexpr auto filename{ "sensors.csv" };

  // Open in file in write mode.
  // Create if doesn't exists, otherwise open in append mode.
  mbed::File file;
  auto err = file.open(&fs, filename, O_WRONLY | O_CREAT | O_APPEND);
  if (err) {
    Serial.print("Error opening file for writing: ");
    Serial.println(err);
    return;
  }

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
  String line;

  // Pre-allocate maxLine bytes for line -> amount tbd
  constexpr size_t maxLine{ bytesPerLine };
  line.reserve(maxLine);

  // create line with relevant sensor data
  line = "timestamp=";
  line += millis();
  line += ",temp=";
  line += temp.value();
  line += ",accelX=";
  line += accel.x();
  line += ",accelY=";
  line += accel.y();
  line += ",accelZ=";
  line += accel.z();
  line += "\r\n";

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
            printFile(file); // replace by function for sending contents via BLE

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