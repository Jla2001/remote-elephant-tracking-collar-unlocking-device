//================================Set Data Device=======================================================================================================================
const String BLEName = "Elephant"; // << change Bluetooth this
const String DevAddress = "FE78F372"; // << change Device address this
const String DevEui = "605CA8FFFE78F372"; // << change Device eui this
//=======================================================================================================================================================




#include <ArduinoBLE.h>

BLEService ledService("19B10010-E8F2-537E-4F6C-D104768A1214");
BLEByteCharacteristic ledCharacteristic("19B10011-E8F2-537E-4F6C-D104768A1214", BLERead | BLEWrite);


bool send_sucress = false;
bool start_send = false;
bool repeat_enable = true;
bool Lora_ready = false;
bool Lorawan_ready = false;
bool Lora_Lock = false;
bool lock = false;

const int l1 = D42;
const int l2 = D43;

int cmm_state = 0;
int Try_count = 0;
int Lora_RebootCount = 0;

unsigned long Data_Timestamp = 0;
unsigned long Lora_previousMillis = 0;
unsigned long Lora_currentMillis = millis();
const long Lora_interval = 5000;

String predata;

struct _RES {
  unsigned char status;
  String data;
  String temp;
};
_RES res_;

void setup() {
  
  pinMode(l1, OUTPUT);
  pinMode(l2, OUTPUT);

  configureSerial1TxRx(D39, AM_HAL_PIN_39_UART1TX, D40, AM_HAL_PIN_40_UART1RX);  // Select the standard ATP pins for UART1 TX and RX
  Serial.begin(115200);
  Serial1.begin(9600, SERIAL_8N1);
  Serial.println("Starting UP...");

  delay(3000);
  Lora_StartUp();

    if (!BLE.begin()) {
      Serial.println("starting BLE failed!");
      while (1);
    }
    // set the local name peripheral advertises
  BLE.setLocalName(BLEName.c_str());
  // set the UUID for the service this peripheral advertises:
  BLE.setAdvertisedService(ledService);
  // add the characteristics to the service
  ledService.addCharacteristic(ledCharacteristic);
  // add the service
  BLE.addService(ledService);
  ledCharacteristic.writeValue(0);
  // start advertising
  BLE.advertise();
  Serial.println("Bluetooth device active, waiting for connections...");
}

void loop() {
  Read_serial();
  
  BLE.poll();
  
  if(ledCharacteristic.value()){
    if(ledCharacteristic.value() == 198){
        Serial.println("Unlock sucress");
        digitalWrite(l1,HIGH);
        digitalWrite(l2,HIGH);
        delay(1000);
        ledCharacteristic.writeValue(11);
        digitalWrite(l1,0);
        digitalWrite(l2,0);
        Serial.println(ledCharacteristic.value());
    }
  }
}

void configureSerial1TxRx(PinName txPin, uint8_t txPinFuncSel, PinName rxPin, uint8_t rxPinFuncSel)  // Configure pins for UART1 TX and RX
{
  am_hal_gpio_pincfg_t pinConfigTx = g_AM_BSP_GPIO_COM_UART_TX;
  pinConfigTx.uFuncSel = txPinFuncSel;
  pin_config(txPin, pinConfigTx);

  am_hal_gpio_pincfg_t pinConfigRx = g_AM_BSP_GPIO_COM_UART_RX;
  pinConfigRx.uFuncSel = rxPinFuncSel;
  pinConfigRx.ePullup = AM_HAL_GPIO_PIN_PULLUP_WEAK;  // Put a weak pull-up on the Rx pin
  pin_config(rxPin, pinConfigRx);
}

void Lora_ComInit(void) {
  if (Send_command("AT").indexOf("+AT: OK") != -1) {
    Serial.println("Module Respond...");
    Send_command("AT+ID=DevAddr,\""+DevAddress+"\"");
    Send_command("AT+ID=DevEui,\""+DevEui+"\"");
    Send_command("AT+KEY=APPSKEY,\"1628AE2B7E15D2A6ABF7CF4F3C158809\"");
    Send_command("AT+KEY=NWKSKEY,\"28AED22B7E1516A609CFABF715884F3C\"");
    Send_command("AT+MODE=LWABP");
    Send_command("AT+DR=AS923");
    Send_command("AT+CH=NUM,0-5,64");
    Send_command("AT+CLASS=C");
    Send_command("AT+POWER=16");
    Send_command("AT+RXWIN1");
    Send_command("AT+RXWIN2");
    Send_command("AT+RETRY=5");
    Send_command("AT+LW=NET,ON");
    Send_command("AT+JOIN=FORCE");
    Lora_ready = true;
  } else {
    Serial.println("Module Not Respond... Restart Again...");
    Lora_RebootCount = Lora_RebootCount + 1;
    if (Lora_RebootCount < 4) {
      delay(1000);
      Lora_ComInit();
    }
    if (Lora_RebootCount >= 4) Lora_ready = false;
  }
}

String Send_command(String cmd) {
  Lora_Lock = true;
  String Sim_res = "";
  int fail_cunt = 0;
  do {
    cmm_state = -1;
    Serial1.println(cmd);
    Sim_res = Wait_module_res(400, "+");  //old set = 500
    if (cmm_state == 1) break;
    if (fail_cunt > 5) break;
    Serial.print(".");
    delay(300);
    fail_cunt++;
  } while (cmm_state != 1);

  //Sim_res.replace("OK", "");
  //Serial.println(Sim_res);
  Lora_Lock = false;
  return (Sim_res);
}

String Wait_module_res(long tout, String str_wait) {
  unsigned long pv_ok = millis();
  unsigned long current_ok = millis();
  String input;
  unsigned char flag_out = 1;
  unsigned char res = -1;
  //_RES res_;
  res_.temp = "";
  res_.data = "";

  while (flag_out) {
    if (Serial1.available()) {
      input = Serial1.readStringUntil('\n');
      res_.temp += input;
      if (input.indexOf(str_wait) != -1) {
        res = 1;
        cmm_state = 1;
        flag_out = 0;
      } else if (input.indexOf(F("ERROR")) != -1) {
        res = 0;
        cmm_state = 0;
        flag_out = 0;
      }
    }
    current_ok = millis();
    if (current_ok - pv_ok >= tout) {
      flag_out = 0;
      res = 0;
      pv_ok = current_ok;
    }
  }
  res_.status = res;
  res_.data = input;
  return (res_.temp);
}

void Read_serial() {
  if (Lora_Lock == false and Serial1.available()) {
    while (Serial1.available() > 0) {
      String text = Serial1.readStringUntil('\n');
      Serial.println(text);
      if (text.indexOf("+MSG: LoRaWAN modem is busy") != -1 or text.indexOf("+MSG: ERROR(-1)") != -1) {
        Lorawan_ready = false;
      }
      if (text.indexOf("+MSG: Link") != -1) {
        Lorawan_ready = true;
        Serial.println("Lorawan Link sucress");
      }
      if (text.indexOf("+MSG: PORT:") != -1) {
        start_send = false;
        send_sucress = true;
        Try_count = 0;
        Serial.println("Send  Data sucress");
      }
      if(text.indexOf("55434B")>0){
        digitalWrite(l1,HIGH);
        digitalWrite(l2,HIGH);
        String playload = "14.159040,101.344346,0,0,0,62.5,3.76,55";
        Send_LorawanData(playload);
        unsigned long Lora_currentMillis = millis();
        if (Lorawan_ready == false and Lora_ready == true and (Lora_currentMillis - Lora_previousMillis >= Lora_interval)) {
          Lorawan_ready = true;
          Lora_previousMillis = Lora_currentMillis;
          if (Send_command("AT+MSG").indexOf("+MSG: Start") != -1) {
            Serial.println("Data Sended to Lorawan");
          }
        }
        delay(2000);
        digitalWrite(l1,0);
        digitalWrite(l2,0);
        Serial.println("Unlock sucress");
      }
    }
  }
}

void Send_LorawanData(String cmd) {
  predata = cmd;
  if (Send_command("AT+MSG=\"" + cmd + "\"").indexOf("+MSG: Start") != -1) {
    start_send = true;
    Data_Timestamp = millis();
//    Serial.println("Data Sended to Lorawan");
  }
}

void Lora_StartUp(void) {
  Lora_ComInit();
  if (Lora_ready == true) {
    Serial.println("Lora Initial successful");
  } else {
    Serial.println("Fatal Fail Lora Not Respond");
  }
}

String HexString2ASCIIString(String hexstring) {
  String temp = "", sub = "", result;
  char buf[3];
  for (int i = 0; i < hexstring.length(); i += 2) {
    sub = hexstring.substring(i, i + 2);
    sub.toCharArray(buf, 3);
    char b = (char)strtol(buf, 0, 16);
    if (b == '\0')
      break;
    temp += b;
  }
  return temp;
}

String getValue(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = { 0, -1 };
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

//  if (Serial.available()>0)
//  {
//    Serial1.write(Serial.read());
//  }
//  if (Serial1.available()>0)
//  {
//    Serial.write(Serial1.read());
//  }
