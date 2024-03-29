#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <Wire.h>

// Replace with your network credentials
const char* ssid = "vivo1920";          //Mekar Jaya Kost FAT
const char* password = "12345679";  //mekar1976 comeondreaming

// Initialize Telegram BOT
#define BOTtoken "6842674514:AAHYIx4q47yu8SETk_Y4QFyK14Uf-7ZSFoc"
#define CHAT_ID "1860319120"

#ifdef ESP8266
X509List cert(TELEGRAM_CERTIFICATE_ROOT);
#endif

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

#include <RTClib.h>
RTC_DS3231 rtc;

// Checks for new messages every 0.1 second.
int botRequestDelay = 100;
unsigned long lastTimeBotRan;

// Ultrasonic
long t;
int trigger = D11;
int echo = D10;
float distance;
float percentageFood;
float max_food = 27.00;

// Servo
#include <Servo.h>
int pin_servo = D1;
Servo myservo;

// Variables for feeding control
bool isFeeding = false;
unsigned long feedingStartTime;
unsigned long feedDuration = 100;

void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    if (chat_id != CHAT_ID) {
      bot.sendMessage(chat_id, "Pakan kucing", "");
      continue;
    }

    String text = bot.messages[i].text;
    Serial.println(text);
    String from_name = bot.messages[i].from_name;

    if (text == "/mulai") {
      String welcome = "Pakan Unggas bersama, " + from_name + "!\n";
      welcome += "Pilih Langkah Berikut:\n\n";
      welcome += "/Pakan - Kasih Makan\n";
      welcome += "/Aturwaktu - Atur waktu (#HH:MM#HH:MM)\n";
      welcome += "/cek - Cek Tersedia Makanan\n";
      bot.sendMessage(chat_id, welcome, "");
    }

    if (text == "/Pakan") {
      if (!isFeeding) {
        isFeeding = true;
        feedingStartTime = millis();
        kasih_pakan(1);
        DateTime lastFeedTime = rtc.now();
        String lastFeedStatus = "Terakhir Memberi Pakan: " + String(lastFeedTime.hour()) + ":" + String(lastFeedTime.minute());
        bot.sendMessage(chat_id, "Berhasil Memberi Pakan!", "");
        bot.sendMessage(chat_id, lastFeedStatus, "");      
      }
    }

    if (text == "/cek") {
      calcRemainingFood();
      bot.sendMessage(chat_id, "Remaining food: " + String(percentageFood) + "% (Jarak: " + String(distance) + " cm).", "");
    }

    // Set the feeding times based on the received message
    if (text.startsWith("/Aturwaktu ")) {
      String timeData = text.substring(10);  // Remove "/settime " from the string
      int delimiterIndex = timeData.indexOf('#');

      if (delimiterIndex != -1 && delimiterIndex > 0 && delimiterIndex < timeData.length() - 1) {
        String time1 = timeData.substring(0, delimiterIndex);   // Get the first feeding time
        String time2 = timeData.substring(delimiterIndex + 1);  // Get the second feeding time

        int hour1 = time1.substring(0, 2).toInt();
        int minute1 = time1.substring(3).toInt();
        int hour2 = time2.substring(0, 2).toInt();
        int minute2 = time2.substring(3).toInt();


        rtc.setAlarm1(DateTime(rtc.now().year(), rtc.now().month(), rtc.now().day(), hour1, minute1, rtc.now().second()), DS3231_A1_Minute);
        rtc.setAlarm2(DateTime(rtc.now().year(), rtc.now().month(), rtc.now().day(), hour2, minute2, rtc.now().second()), DS3231_A2_Minute);


        String successMsg = "Berhasil Mengatur Waktu.\n";
        successMsg += "Makan 1 = " + time1 + "\n";
        successMsg += "Makan 2 = " + time2;
        bot.sendMessage(chat_id, successMsg, "");
      } else {
        bot.sendMessage(chat_id, "Invalid time format. Please use the format 'HH:MM#HH:MM'", "");
      }
    }
  }
}

void setup() {
  Serial.begin(9600);

#ifdef ESP8266
  configTime(0, 0, "pool.ntp.org");
  client.setTrustAnchors(&cert);
#endif

  // Connect to Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }

  // Initialize RTC
  Wire.begin();
  rtc.begin();
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  // Ultrasonic
  pinMode(trigger, OUTPUT);
  pinMode(echo, INPUT);

  // Servo
  myservo.attach(pin_servo);
  myservo.write(0);  // Initialize the servo position
}

void calcRemainingFood() {
  digitalWrite(trigger, LOW);
  delayMicroseconds(2);
  digitalWrite(trigger, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigger, LOW);
  t = pulseIn(echo, HIGH) / 2;
  if (t == 0.00) {
    return;
  }
  distance = float(t * 0.0343);
  percentageFood = 100 - ((100 / max_food) * distance);
  if (percentageFood < 0.00) {
    percentageFood = 0.00;
  }
  Serial.print("Makanan Tersisa: ");
  Serial.print(percentageFood);
  Serial.println("%");
  delay(10);
}

void kasih_pakan(int jumlah) {
  for (int a = 1; a <= jumlah; a++) {
    myservo.write(180);
    delay(1000);
    myservo.write(0);
  }
}

void loop() {
  calcRemainingFood();
  if (millis() > lastTimeBotRan + botRequestDelay) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages) {
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTimeBotRan = millis();
  }

  if (isFeeding) {
    if (millis() - feedingStartTime >= feedDuration) {
      isFeeding = false;

      // Update the last feeding time in the RTC
      DateTime now = rtc.now();
      rtc.adjust(DateTime(now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second()));
    }
  }

  // Check if it's time for feeding 1 or feeding 2
  DateTime now = rtc.now();
  DateTime feedTime1 = rtc.getAlarm1();
  DateTime feedTime2 = rtc.getAlarm2();

  if (now.hour() == feedTime1.hour() && now.minute() == feedTime1.minute() && now.second() < 10) {
    // Waktunya untuk makan 1 dan detik kurang dari 10
    if (!isFeeding) {
      isFeeding = true;
      feedingStartTime = millis();
      kasih_pakan(1);
      bot.sendMessage(CHAT_ID, "Makanan 1 diberikan.", "");
    }
  } else if (now.hour() == feedTime2.hour() && now.minute() == feedTime2.minute() && now.second() < 10) {
    // Waktunya untuk makan 2 dan detik kurang dari 10
    if (!isFeeding) {
      isFeeding = true;
      feedingStartTime = millis();
      kasih_pakan(1);
      bot.sendMessage(CHAT_ID, "Makanan 2 diberikan.", "");
    }
  }
}
