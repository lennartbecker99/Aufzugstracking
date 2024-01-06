using System;
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

        private static async void Characteristic_CharacteristicValueChanged(object sender, GattCharacteristicValueChangedEventArgs e)
        {
            Console.WriteLine(System.Text.Encoding.UTF8.GetString(e.Value));
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

            // Ausgabe des verbunden Geräts
            if (bleDevice != null)
            {
                Console.WriteLine(bleDevice.Name);
                Console.WriteLine(bleDevice.Id);
            }
            else
            {
                Console.WriteLine("no device connected");
            }

            // Services anzeigen lassen
            var gatt = bleDevice.Gatt;
            Console.WriteLine("Connecting to GATT Server...");
            await gatt.ConnectAsync();
            Console.WriteLine("connected");
            Console.WriteLine(BluetoothUuid.GetCharacteristic("battery_level"));
            var services = await gatt.GetPrimaryServicesAsync();
            //var serviceTest = await gatt.GetPrimaryServiceAsync(new Guid("0008"));
            //if (serviceTest != null)
            Console.Write("got services: ");
            Console.WriteLine(services.Count);
            foreach (GattService service in services)
            {
                Console.Write("Service Uuid: ");
                Console.WriteLine(service.Uuid);
                var characteristics = await service.GetCharacteristicsAsync();
                
                foreach (GattCharacteristic charac in characteristics)
                {
                    Console.Write("Characteristic Uuid: ");
                    Console.Write(charac.Uuid);

                    byte[] valArray = await charac.ReadValueAsync();
                    Console.Write(" || value: ");
                    if (valArray != null)
                    {
                        val = System.Text.Encoding.UTF8.GetString(valArray);
                        Console.WriteLine(val.ToString());
                    }
                    else
                    {
                        Console.WriteLine("null");
                    }
                    if (service.Uuid.ToString() == "0008" & charac.Uuid.ToString() == "0007")
                    {
                        Console.Write("Characteristic Properties: ");
                        Console.WriteLine(charac.Properties);


                        //characteristic.CharacteristicValueChanged += Characteristic_CharacteristicValueChanged;
                        charac.CharacteristicValueChanged += (s, e) =>
                        {
                            writing = true;
                            val = System.Text.Encoding.UTF8.GetString(e.Value);
                            if (val == "ende") { transfer = false; }
                            else { Console.WriteLine(val); }
                            writing = false;
                        };

                        Console.WriteLine("will start notifications");
                        await charac.StartNotificationsAsync();

                        while (transfer)
                        {
                            while (writing) ;
                            
                            await charac.WriteValueWithResponseAsync(w);
                            Console.WriteLine("wrote value");
                        }
                    }
                }
            }
            Console.WriteLine("warte auf Programmende");
            // Verzögerung vor Programmende
            Thread.Sleep(5000);
            gatt.Disconnect();
        }
    }
}