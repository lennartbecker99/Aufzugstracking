#include <BlockDevice.h>
#include <Dir.h>
#include <File.h>
#include <LittleFileSystem.h>




// Name of filesystem root on which files will be deleted
constexpr auto userRoot{ "storedSensorData" };

mbed::BlockDevice* spiflash;
mbed::LittleFileSystem fs{ userRoot };

String baseDirName = "/";
int amountOfFiles = 0;

// milliseconds to wait before deleting all files
const int waitingTime = 30000;



void setup() {
  Serial.begin(115200);

  // wait for serial startup for max. 2,5s
  for (const auto timeout = millis() + 2500; !Serial && millis() < timeout; delay(250))
    ;

  Serial.print("Loading SPI Flash Storage and LittleFS filesystem...");

  // init core-wide instance of SPIF Block Device
  spiflash = mbed::BlockDevice::get_default_instance();
  spiflash->init();

  // Mount the filesystem
  int err = fs.mount(spiflash);
  if (err) {
    err = fs.reformat(spiflash);
    Serial.print("Error mounting file system: ");
    Serial.println(err);
    while (true)
      // do nothing if filesystem was not mounted correctly
      ;
  }
  Serial.println(" done.");


  // inform about available files
  baseDirName += userRoot;
  amountOfFiles = listDirectoriesAndFiles();
  if (amountOfFiles > 0) {
    Serial.print(amountOfFiles);
    Serial.println(" have been discovered.");
  } else {
    Serial.println("No files have been discovered: No more actions.");
    while (true) {
      // do nothing if no files available
      ;
    }
  }

  Serial.print("Waiting for ");
  Serial.print(waitingTime);
  Serial.println("sec before deleting all files...");

  // inform user about left time before file deletion
  const auto startMillis = millis();
  for (const auto timeout = startMillis + waitingTime; millis() < timeout; delay(5000)) {
    Serial.print(millis() - startMillis);
    Serial.println(" seconds left before deleting files...");
  }


  //delete all files
  amountOfFiles = deleteFiles();
  if (amountOfFiles > 0) {
    Serial.print(amountOfFiles);
    Serial.println(" files deleted.");
  } else {
    Serial.println("No files deleted.");
  }
}




void loop() {

  // wait for 5s
  long startMillis = millis();
  while (millis() - startMillis < 5000)
    ;

  Serial.println("looping without activities...");
}




int listDirectoriesAndFiles() {

  Serial.print("Listing files on ");
  Serial.println(baseDirName);

  // Open filesystem root
  mbed::Dir dir(&fs, "/");
  dirent ent;

  int fileCount = 0;

  // Cycle through all the directory entries
  while ((dir.read(&ent)) > 0) {
    switch (ent.d_type) {

      case DT_DIR:
        {
          Serial.print("Directory: ");
          Serial.println(ent.d_name);
          break;
        }

      case DT_REG:
        {
          Serial.print("Regular File: ");
          Serial.println(ent.d_name);

          fileCount += 1;

          break;
        }
      default:
        {
          Serial.print("Not directory nor file.");
          break;
        }
    }
  }

  return fileCount;
}


int deleteFiles() {


  Serial.print("Deleting files on ");
  Serial.println(baseDirName);

  // Open filesystem root
  mbed::Dir dir(&fs, "/");
  dirent ent;

  int fileCount = 0;

  // Cycle through all the directory entries
  while ((dir.read(&ent)) > 0) {
    if (ent.d_type == DT_REG) {

      mbed::File file;

      // truncate file to length zero before  deletion
      auto ret = file.open(&fs, ent.d_name, O_TRUNC);
      // don't delete file if not truncated
      if (ret < 0) {
        Serial.print("Unable to truncate file ");
        Serial.println(ent.d_name);
      } else {
        file.close();
        fs.remove(ent.d_name);

        fileCount += 1;
      }
    }
  }

  return fileCount;
}