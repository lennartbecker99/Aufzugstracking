using System;
using System.IO;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using InTheHand.Bluetooth;

namespace ConsoleApp1
{
    class Program
    {
        private static byte[] w = new byte[1];
        private static GattCharacteristic characteristic = null;
        private static bool transfer = true;
        private static bool writing = false;
        private static string val = null;
        private static string filepath = "daten.csv";
        private static FileStream fs = null;
        private static StreamWriter writer = null;
        private static string uuid_service = "0008";
        private static string uuid_characteristic = "0007";

        private static async void Characteristic_CharacteristicValueChanged(object sender, GattCharacteristicValueChangedEventArgs e)
        {
            Console.WriteLine(Encoding.UTF8.GetString(e.Value));
            await characteristic.WriteValueWithResponseAsync(w);
            Console.WriteLine("wrote value");
        }


        public static async Task Main(string[] args)
        {

            //https://inthehand.com/2022/12/21/12-days-of-bluetooth-8-bluetooth-low-energy-in-code/

            // Filter für Suche nach BLE-Geräten erzeugen
            BluetoothLEScanFilter filter_prefix = new BluetoothLEScanFilter();
            filter_prefix.NamePrefix = "";    // nur Geräte, deren Name das angegebene Präfix hat

            // Optionen für Geräteanfrage erzeugen & Filter hinzufügen
            RequestDeviceOptions opts = new RequestDeviceOptions();
            opts.Filters.Add(filter_prefix);

            // Geräteabfrage erzeugen
            BluetoothDevice bleDevice = await Bluetooth.RequestDeviceAsync(opts);

            // Ausgabe des verbunden Geräts, falls vorhanden, sonst Anwendung beenden
            if (bleDevice != null)
            {
                Console.WriteLine($"Connected to device {bleDevice.Name} with Id {bleDevice.Id}");
            }
            else
            {
                Console.WriteLine("no device connected");
                Environment.Exit(1);
            }

            // Verbindung zu Server
            var gatt = bleDevice.Gatt;
            Console.WriteLine("Connecting to GATT Server...");
            await gatt.ConnectAsync();
            Console.WriteLine($"Connected to server from device {gatt.Device}");


            // Services anzeigen
            var services = await gatt.GetPrimaryServicesAsync();
            Console.WriteLine($"{services.Count} services available");

            foreach (GattService service in services)
            {
                Console.WriteLine($"Service with Uuid {service.Uuid}");

                // Charakteristiken pro Service anzeigen
                var characteristics = await service.GetCharacteristicsAsync();
                foreach (GattCharacteristic charac in characteristics)
                {
                    Console.Write($"Characteristic with Uuid {charac.Uuid}");

                    byte[] valArray = await charac.ReadValueAsync();
                    if (valArray != null)
                    {
                        val = Encoding.UTF8.GetString(valArray);
                        Console.WriteLine($" has value {val}");
                    }
                    else
                    {
                        Console.WriteLine("null");
                    }

                    // Subscription des gewünschten Services
                    if (service.Uuid.ToString() == uuid_service & charac.Uuid.ToString() == uuid_characteristic)
                    {
                        // Properties anzeigen
                        Console.WriteLine($"Characteristic Properties: {charac.Properties}");

                        // Callback für Wertänderung: neuen Wert in Konsole/Puffer schreiben
                        charac.CharacteristicValueChanged += (s, e) =>
                        {
                            writing = true;
                            val = Encoding.UTF8.GetString(e.Value);
                            // set transfer-flag when last filepart received
                            if (val == "ende") { transfer = false; }
                            //
                            else
                            {
                                Console.WriteLine(val);
                                writer.WriteLine(val);
                            }
                            writing = false;
                        };

                        filepath = val + ".csv";
                        fs = new FileStream(filepath, FileMode.Create);
                        writer = new StreamWriter(fs, Encoding.UTF8);
                        // Subscription
                        Console.WriteLine($"start notifications of characteristic {charac.Uuid}");
                        await charac.StartNotificationsAsync();

                        while (transfer)
                        {
                            while (writing) { Console.Write("-"); } // Timer einbauen für Abbruch

                            await charac.WriteValueWithResponseAsync(w);
                            Console.WriteLine("wrote value");
                            Thread.Sleep(1000);
                            //writing = true;
                        }
                    }
                }
            }

            if (writer != null)
                writer.Close();
            if (fs != null)
                fs.Dispose();

            Console.WriteLine("warte auf Programmende");
            Thread.Sleep(5000);
            gatt.Disconnect();
        }
    }
}