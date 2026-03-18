// -------------------------------------------------------------
// Separate sketch to receive messages from SIM7600 GSM Module
// -------------------------------------------------------------
// Based on the main sketch, the SIM7600 receives/transmits on:
// RX -> RX1 (Pin 19)
// TX -> TX1 (Pin 18)
// Meaning we use Serial1 for the GSM module (Hardware Serial on Mega).
// -------------------------------------------------------------

#define sim7600Serial Serial1

// Optional: LED pin to indicate message received, or buzzer
const int ledPin = 13;

// String incomingMessage = "";

void setup() {
  Serial.begin(115200);
  while (!Serial) {;}
  sim7600Serial.begin(115200);
  pinMode(ledPin, OUTPUT);
  Serial.println("Initializing SIM7600 for Robust SMS Reception...");
  
  // Give module 5 seconds to power on
  delay(5000);

  // Send basic AT until OK (Module is alive)
  Serial.println("Checking connection...");
  while(!sendATCommand("AT", "OK", 1000)) {
     Serial.println("Waiting for AT response...");
     delay(1000);
  }

  // Wait for SIM card to be ready
  Serial.println("Checking SIM card status...");
  while(!sendATCommand("AT+CPIN?", "READY", 1000)) {
     Serial.println("Waiting for SIM to be READY...");
     delay(2000);
  }

  // Wait for Network Registration (0,1 or 0,5)
  Serial.println("Waiting for Network Registration...");
  bool registered = false;
  for(int i=0; i<30; i++) { // Wait up to 60 seconds
     String resp = sendATCommandReturn("AT+CREG?", 1000);
     if(resp.indexOf("0,1") != -1 || resp.indexOf("0,5") != -1 || resp.indexOf("1,1") != -1 || resp.indexOf("1,5") != -1) {
        registered = true;
        break;
     }
     Serial.println("Module searching for network...");
     delay(2000);
  }

  if(registered) {
    Serial.println("Network Registered successfully!");
  } else {
    Serial.println("Warning: Network might not be registered yet. Proceeding anyway.");
  }
  
  // Extra delay just to be safe
  delay(2000);
  
  // Set memory storage to Mobile Equipment (ME) instead of SIM
  sendATCommand("AT+CPMS=\"ME\",\"ME\",\"ME\"", "OK", 2000);
  // Set Character Set to GSM
  sendATCommand("AT+CSCS=\"GSM\"", "OK", 1000);
  // Set SMS Text Mode
  sendATCommand("AT+CMGF=1", "OK", 1000);
  // Set SMS Text Mode Parameters (crucial for some module firmware)
  sendATCommand("AT+CSMP=17,167,0,0", "OK", 1000);

  Serial.println("Clearing old SMS...");
  sendATCommand("AT+CMGDA=\"DEL ALL\"", "OK", 3000);
  
  // Set CNMI to ping with "+CMTI" when SMS is stored
  sendATCommand("AT+CNMI=2,1", "OK", 1000);
  
  Serial.println("SIM7600 Ready. Waiting for messages...");
}

void loop() {
  if (sim7600Serial.available()) {
    String incoming = sim7600Serial.readStringUntil('\n');
    incoming.trim();
    if (incoming.length() > 0) {
      Serial.println("[SIM7600] " + incoming);
      
      // Look for SMS notification (e.g. +CMTI: "ME",1)
      if (incoming.startsWith("+CMTI:")) {
        int commaIndex = incoming.indexOf(',');
        if (commaIndex != -1) {
          String indexStr = incoming.substring(commaIndex + 1);
          int smsIndex = indexStr.toInt();
          readAndProcessSMS(smsIndex);
        }
      } 
      else if (incoming.startsWith("RING")) {
         Serial.println("---> Incoming voice call detected! (Module isn't deaf)");
      }
    }
  }

  // Forward debugging AT commands typed in Serial Monitor to SIM7600
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    if (command.length() > 0) {
      sim7600Serial.println(command);
    }
  }
}

// Helper to forcefully wait for a specific response with a timeout
bool sendATCommand(String cmd, String expectedResponse, unsigned long timeout) {
  // Clear the buffer
  while(sim7600Serial.available()) { sim7600Serial.read(); }
  
  sim7600Serial.println(cmd);
  unsigned long t = millis();
  String response = "";
  
  while(millis() - t < timeout) {
    if(sim7600Serial.available()){
      char c = sim7600Serial.read();
      response += c;
      Serial.write(c);
      if(response.indexOf(expectedResponse) != -1) {
        return true;
      }
    }
  }
  return false;
}

// Helper to send AT command and return the full string response
String sendATCommandReturn(String cmd, unsigned long timeout) {
  // Clear the buffer
  while(sim7600Serial.available()) { sim7600Serial.read(); }
  
  sim7600Serial.println(cmd);
  unsigned long t = millis();
  String response = "";
  
  while(millis() - t < timeout) {
    if(sim7600Serial.available()){
      char c = sim7600Serial.read();
      response += c;
      Serial.write(c);
    }
  }
  return response;
}

// Read SMS from the notified index, process it, then delete it.
void readAndProcessSMS(int index) {
  Serial.print("Reading SMS at memory index: ");
  Serial.println(index);
  
  // Force a 3-second wait to capture the ENTIRE message including the body and the "OK" at the end
  // The module sends the header, then a newline, then the body, then "OK".
  String smsContent = sendATCommandReturn("AT+CMGR=" + String(index), 3000);
  
  Serial.println("--- SMS Content ---");
  Serial.println(smsContent);
  Serial.println("-------------------");
  
  processReceivedMessage(smsContent);
  
  // Delete the SMS after reading so the memory never fills up
  Serial.print("Deleting SMS at index: ");
  Serial.println(index);
  String delResp = sendATCommandReturn("AT+CMGD=" + String(index), 2000);
  Serial.println(delResp);
}

// -------------------------------------------------------------
// Processing Logic
// -------------------------------------------------------------
void processReceivedMessage(String message) {
  message.toUpperCase();
  if (message.indexOf("OPEN DOOR") != -1) {
    Serial.println("Command 'OPEN DOOR' recognized. Triggering relay...");
    digitalWrite(ledPin, HIGH);
    delay(1000);
    digitalWrite(ledPin, LOW);
  } else if (message.indexOf("STATUS") != -1) {
    Serial.println("Command 'STATUS' recognized. Add logic to send reply.");
  } else {
    // Other parts of the AT sequence are inside this string
    Serial.println("Note: Message processed. No known commands recognized.");
  }
}

// Example function to send an SMS reply
void sendSMS(String phoneNumber, String textMessage) {
  Serial.println("Sending SMS to: " + phoneNumber);
  sim7600Serial.print("AT+CMGS=\"");
  sim7600Serial.print(phoneNumber);
  sim7600Serial.println("\"");
  delay(1000);
  sim7600Serial.print(textMessage);
  delay(100);
  sim7600Serial.write(26); // Ctrl+Z
  delay(5000); 
  while(sim7600Serial.available()) {
    Serial.write(sim7600Serial.read());
  }
}
