using System;
using System.IO;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using InTheHand.Bluetooth;

namespace Aufzugtracking_Datenempfang
{
    internal class Program  //"internal" entfernen?
    {
        private static byte[] w = new byte[1];
        private static bool transfer = true;
        private static bool writing = false;
        private static bool dateTransferred = false;
        private static bool transferSuccess = false;
        private static string val = null;
        private static string filename = "noFileReceived";
        private static FileStream fs = null;
        private static StreamWriter writer = null;
        private static string uuid_service = "0008";
        private static string uuid_characteristic = "0007";



        static async Task Main(string[] args)
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
                            // set transfer success-flag when last filepart received
                            if (val == "ende") { transferSuccess = true; }
                            //
                            else
                            {
                                Console.WriteLine(val);
                                writer.Write(val);
                            }
                            writing = false;
                        };

                        string dt_year = DateTime.Now.ToString("yyyy");
                        string dt_month = DateTime.Now.ToString("MM");
                        string dt_day = DateTime.Now.ToString("dd");
                        string dt_hour = DateTime.Now.ToString("HH");
                        string dt_minute = DateTime.Now.ToString("mm");
                        string directoryName = dt_year + dt_month + dt_day + "_" + dt_hour + "h" + dt_minute + "m\\";
                        filename = "C:\\Aufzugtracking_Dateien\\" + directoryName + val;
                        Directory.CreateDirectory(Path.GetDirectoryName(filename));
                        fs = new FileStream(filename, FileMode.Append);
                        writer = new StreamWriter(fs, Encoding.UTF8);

                        // aktuelles Datum + Zeit übertragen
                        string datetime = DateTime.Now.ToString("MM / dd / yyyy HH: mm");
                        byte[] w_datetime = Encoding.ASCII.GetBytes(datetime);
                        await charac.WriteValueWithResponseAsync(w_datetime);
                        dateTransferred = true;

                        // Subscription
                        Console.WriteLine($"start notifications of characteristic {charac.Uuid}");
                        await charac.StartNotificationsAsync();

                        while (transfer)
                        {
                            while (writing) { Console.Write("-"); }

                            if (dateTransferred)
                            {
                                await charac.WriteValueWithResponseAsync(w);
                            }

                            if (transferSuccess) break;
                        }
                    }
                }
            }

            writer?.Close();
            fs?.Dispose();
            gatt.Disconnect();

            Console.WriteLine($"data stored: {filename}.");
            Thread.Sleep(5000);
        }
    }
}