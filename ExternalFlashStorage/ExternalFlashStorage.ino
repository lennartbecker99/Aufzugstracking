// basierend auf Beispielcode aus lib "Arduino_BHY2": "StandaloneFlashStorage"

/*
    This sketch shows how to use Nicla in standalone mode and how to save data
    the on-board 2MB SPI Flash using the LitteFS filesystem.

    Please, take care both ot RAM and Flash usage at run-time.

    This example shows how to use MbedOS-native Storage APIs
    - https://os.mbed.com/docs/mbed-os/v6.15/apis/data-storage.html

    C/C++ storage APIs are supporte too
    - C-based STDIO: https://en.cppreference.com/w/cpp/header/cstdio
    - Stream-based: https://en.cppreference.com/w/cpp/io
*/

#include <BlockDevice.h>
#include <Dir.h>
#include <File.h>
#include <FileSystem.h>
#include <LittleFileSystem.h>
#include <Arduino_BHY2.h>

// Name of filesystem root
constexpr auto userRoot { "storedSensorData" };

mbed::BlockDevice* spif;
mbed::LittleFileSystem fs { userRoot };


// sensor objects to be used
SensorXYZ accel(SENSOR_ID_ACC);
SensorXYZ gyro(SENSOR_ID_GYRO);
Sensor temp(SENSOR_ID_TEMP);
Sensor gas(SENSOR_ID_GAS);

const int storeIntervall = 1000;
const int printIntervall = 5000;
const int statsIntervall = 10000;




void setup()
{
    Serial.begin(115200);

    // wait for serial startup for max. 2,5s
    for (const auto timeout = millis() + 2500; !Serial && millis() < timeout; delay(250))
        ;

    Serial.print("Loading the SPI Flash Storage and LittleFS filesystem...");

    // init core-wide instance of SPIF Block Device
    spif = mbed::BlockDevice::get_default_instance();
    spif->init();

    // Mount the filesystem
    int err = fs.mount(spif);
    if (err) {
        err = fs.reformat(spif);
        Serial.print("Error mounting file system: ");
        Serial.println(err);
        while (true)
            // do nothing if filesystem was not mounted correctly
            ;
    }
    Serial.println(" done.");

    Serial.print("Initialising the sensors... ");
    BHY2.begin();

    accel.begin();
    gyro.begin();
    temp.begin();
    gas.begin();
    Serial.println(" done.");
}

void loop()
{
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

    // List files on the Flash Memory and print contents after specified Intervall
    if (millis() - printTime >= printIntervall) {
        printTime = millis();
        listDirsAlt();
    }

    // Retrieve and print Flash Memory stats after specified Intervall
    if (millis() - statsTime >= statsIntervall) {
        statsTime = millis();
        printStats();
    }
}




void storeData()
{
    // Append data to file on LittleFS filesystem
    constexpr auto filename { "sensors.csv" };

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
    file.close();
}

String sensorsToCSVLine()
{
    String line;

    // Pre-allocate maxLine bytes for line
    constexpr size_t maxLine { 128 };
    line.reserve(maxLine);

    line = "timestamp=";
    line += millis();
    line += ", accelX=";
    line += accel.x();
    line += ", accelY=";
    line += accel.y();
    line += ", accelZ=";
    line += accel.z();
    line += ", gyroX=";
    line += gyro.x();
    line += ", gyroY=";
    line += gyro.y();
    line += ", gyroZ=";
    line += gyro.z();
    line += ", temp=";
    line += temp.value();
    line += ",gas=";
    line += gas.value();
    line += "\r\n";

    return line;
}


void listDirsAlt()
{
    String baseDirName = "/";
    baseDirName += userRoot;

    Serial.print("Listing file on ");
    Serial.print(baseDirName);
    Serial.println(" Filesystem");

    // Open the root of the filesystem
    mbed::Dir dir(&fs, "/");
    dirent ent;

    // Cycle through all the directory entries
    while ((dir.read(&ent)) > 0) {
        switch (ent.d_type) {
        case DT_DIR: {
            Serial.print("Directory ");
            Serial.println(ent.d_name);
            break;
        }
        case DT_REG: {
            Serial.print("Regular File ");
            Serial.print(ent.d_name);

            // Declare and open the file in read-only mode
            mbed::File file;
            auto ret = file.open(&fs, ent.d_name);  // returns <0 if failure
            if (ret) {
                Serial.println("Unable to open file");
                continue;
            }
            Serial.print(" [");
            Serial.print(file.size());
            Serial.println(" bytes]");

            if (file.size() > 0) {
                // Print file with an ad-hoc function. YMMV.
                printFile(file);

                // Empty file after reading all the content. YMMV.
                file.close();
                /*ret = file.open(&fs, ent.d_name, O_TRUNC);
                if (ret < 0)
                    Serial.println("Unable to truncate file");
            } else {
                // Remove file if empty. YMMV.
                file.close();
                fs.remove(ent.d_name);*/
            }

            break;
        }
        default: {
            Serial.print("Other ");
            break;
        }
        }
    }
}

void printStats()
{
    struct statvfs stats { };
    fs.statvfs(userRoot, &stats);

    auto blockSize = stats.f_bsize;

    Serial.print("Total Space [Bytes]:  ");
    Serial.println(stats.f_blocks * blockSize);
    Serial.print("Free Space [Bytes]:   ");
    Serial.println(stats.f_bfree * blockSize);
    Serial.print("Used Space [Bytes]:   ");
    Serial.println((stats.f_blocks - stats.f_bfree) * blockSize);
    Serial.println();
}

void printFile(mbed::File& file)
{
    // Read and print file len-bytes at time
    // to preserve RAM
    constexpr size_t len { 256 };

    size_t totalLen { file.size() };

    while (totalLen > 0) {
        char buf[len] {};

        auto read = file.read(buf, len);
        totalLen -= read;
        for (const auto& c : buf)
            Serial.print(c);
    }
    Serial.println();
}
