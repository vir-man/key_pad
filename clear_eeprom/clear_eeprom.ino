#include "EEPROM.h"
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  clear_eeprom();
}

void loop() {
  // put your main code here, to run repeatedly:

}

void clear_eeprom()
{
  for (int i = 0; i < EEPROM.length(); i++)
  {
    Serial.print("clearing eeprom at address");
    Serial.println(i);
    EEPROM.write(i, 0);
  }
}
