# komoot-eink-navigator
Bluetooth Low Energy companion display for Komoot app

Hardware : LilyGO T5 v2.3.1_2.13 2.13"  ESP32 eink dev board and a 3d printed case
install LilyGO T5 v2.3.1_2.13 2.13 for arduino user : https://github.com/Xinyuan-LilyGO/LilyGo-T5-Epaper-Series?spm=a2g0o.detail.1000023.1.79987bb0ULPo6j  

How to use the device :

1.Important step:

Edit this file: `libraries/BLE/src/BLERemoteCharacteristic.cpp`

Remove lines in the `registerForNotify` function before the `else` statement:

```cpp
uint8_t val[] = {0x01, 0x00};
if(!notifications) val[0] = 0x02;
BLERemoteDescriptor* desc = getDescriptor(BLEUUID((uint16_t)0x2902));
desc->writeValue(val, 2);
```

Komoot does not use the `0x2902` descriptor convention

How to use the device :

2. Upload the ino program:
![image](https://user-images.githubusercontent.com/20805763/184874755-e7740692-4c57-4012-bc63-e487796c6ea8.png)

//Pairing

3. Open the komoot app and navigate to profile>settings>Ble connect. tap on cyclo-hac and turn on esp32


4. Go back and start the navigation under plan and the display will start updating !

![alt text](IMG_20210917_140115.jpg)

Why this design?

The navigation app keeps the phone display ON during operation. It is hard on the battery life especially when we need to keep it maximum brightness for visibility in outdoor conditions. This companion device solves both these issues by implementing a low power design that has greater visibility in bright outdoor conditions.


