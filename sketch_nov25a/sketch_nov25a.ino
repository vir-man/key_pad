void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("Serial started..!");
}

void loop() {
  if(Serial.available()){
    char a =Serial.read();
    Serial.print(a);
  }
  // put your main code here, to run repeatedly:

}
