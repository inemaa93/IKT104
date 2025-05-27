/**
 * @file main.cpp
 * @author Krister Sørstrand
 */
#define BLINKING_RATE 500ms

#include "DFRobot_RGBLCD1602.h"
#include "HTS221Sensor.h"
#include "ipgeolocation_ca_cert.h"
#include "json.hpp"
#include "mbed.h"
#include "weather_ca_cert.h"
#include <chrono>
#include <iostream>
#include <stdio.h>

#include "EthernetInterface.h"
#include "TLSSocket.h"
#include "bbc_ca_cert.h"
#include "mbed.h"

#define BUFFER_SIZE 512
#define SCROLL_SPEED 200ms
#define CHUNK_SIZE 500

#ifdef LED1
DigitalOut led(LED1);
#else
bool led;
#endif

std::string json_var(nlohmann::json document, std::string path) {
  std::string location;
  if (document["geo"][path].is_string()) {
    location = document["geo"][path].get<std::string>();
  }
  return location;
}

std::string json_var2(nlohmann::json document, std::string path) {
  std::string location;
  if (document["location"][path].is_string()) {
    location = document["location"][path].get<std::string>();
  }
  return location;
}

volatile int state = 0;
volatile int b2_pressed = 0;
bool alarm_set = false;
bool alarm_active = false;
volatile int a_minutes = 0;
volatile int a_hours = 0;
bool muted = false;
bool snooze = false;
bool auto_snooze = false;

InterruptIn button1(A0, PullUp);
InterruptIn button2(A1, PullUp);
InterruptIn button3(A2, PullUp);
InterruptIn button4(A3, PullUp);
DigitalIn button5(D0, PullUp);

std::chrono::microseconds auto_mute_counter = 600s;//ten minutes
std::chrono::microseconds snooze_counter = 300s;//5 minutes

PwmOut Buzzer(D5);

Timeout timer;
Timeout snooze_timer;

I2C lcdI2C(D14, D15);
DFRobot_RGBLCD1602 lcd(&lcdI2C, RGB_ADDRESS_V20_7BIT);
DevI2C i2c(PB_11, PB_10);
HTS221Sensor sensor(&i2c);

NetworkInterface *network = nullptr;
EventQueue mainQueue; // Create EventQueue for main tasks
Thread mainThread;    // Create Thread for main tasks
EventQueue rssQueue;  // Create EventQueue for RSS updates
Thread rssThread;     // Create Thread for RSS updates

void call_back1(void) {
  state++;
  if (state == 4) {
    state = 0;
  }
}

void call_back2(void) { b2_pressed = !b2_pressed; }

void call_back3(void) {
  a_hours++;
  if (a_hours == 24) {
    a_hours = 0;
  }
}

void call_back4(void) {
  a_minutes++;
  if (a_minutes == 60) {
    a_minutes = 0;
  }
}

void mute_buzzer(void) { muted = true; }

void snooze_over() {
  snooze = false;
  alarm_active = true;
  Buzzer.write(0.5);
  auto_mute_counter = 10s;
  timer.attach(&mute_buzzer, auto_mute_counter);
}

void snooze_buzzer(void) {
  if (!muted) {
    snooze = true;
    alarm_active = false;
    snooze_timer.attach(&snooze_over, snooze_counter);
    Buzzer.write(0);
    timer.detach();
  }
}

void confirm_alarm(void) {
  b2_pressed = 0;
  alarm_set = true;
  muted = false;
  button2.fall(&call_back2);
  button3.fall(&mute_buzzer);
  button4.fall(&snooze_buzzer);
}

void deactivate(void) { alarm_set = !alarm_set; }


char source[256] = {0};
char headlines[3][256] = {{0}};
int headline_count = 0;

void showonLCD(volatile int &state) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.printf("%s", source);

  char scrolling_headlines[1024] = {0};
  for (int i = 0; i < headline_count; ++i) {
    if (i > 0) {
      strcat(scrolling_headlines, " --- ");
    }
    strcat(scrolling_headlines, headlines[i]);
  }

  while (true) {
    int keep_going = 0;
    for (size_t i = 0; i < strlen(scrolling_headlines) - 15; ++i) {
      if (button5 == 0) {
        keep_going = 1;
        break;
      }
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.printf("%s", source);
      lcd.setCursor(0, 1);
      char display[17] = {0};
      strncpy(display, scrolling_headlines + i, 16);
      lcd.printf("%s", display);
      ThisThread::sleep_for(SCROLL_SPEED / 2);
    }
    if (keep_going == 1) {
      break;
    }
  }
}

void parseBBC(char *data, size_t length) {
  data[length] = '\0';

  if (strlen(source) == 0) {
    char *source_start = strstr(data, "<channel>");
    if (source_start) {
      source_start = strstr(source_start, "<title><![CDATA[");
      if (source_start) {
        source_start += strlen("<title><![CDATA[");
        char *source_end = strstr(source_start, "]]></title>");
        if (source_end) {
          strncpy(source, source_start, source_end - source_start);
          source[source_end - source_start] = '\0';
        }
      }
    }
  }

  while ((headline_count < 3) && (data = strstr(data, "<item>"))) {
    data = strstr(data, "<title><![CDATA[");
    if (data) {
      data += strlen("<title><![CDATA[");
      char *title_end = strstr(data, "]]></title>");
      if (title_end) {
        *title_end = '\0';
        strncpy(headlines[headline_count], data, title_end - data);
        headlines[headline_count][title_end - data] = '\0';
        headline_count++;
        data = title_end + strlen("]]></title>");
      }
    }
  }
}

void hentefeed(NetworkInterface *network, const char *url,
               volatile int &state) {
  TLSSocket socket;
  socket.set_root_ca_cert(CERTIFICATE1);
  socket.set_timeout(5000);
  socket.open(network);

  const char *host_start = strstr(url, "://") ? strstr(url, "://") + 3 : url;
  const char *path_start = strchr(host_start, '/');

  char host[256];
  char path[256];
  strncpy(host, host_start, path_start - host_start);
  host[path_start - host_start] = '\0';
  strcpy(path, path_start);

  SocketAddress address;
  network->gethostbyname(host, &address);
  address.set_port(443);
  socket.connect(address);

  char http_request[512];
  snprintf(http_request, sizeof(http_request),
           "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", path,
           host);
  socket.send(http_request, strlen(http_request));

  char buffer[CHUNK_SIZE];
  size_t total_received = 0;
  while (total_received < CHUNK_SIZE * 7 && headline_count < 3) {
    memset(buffer, 0, sizeof(buffer));
    nsapi_size_or_error_t result = socket.recv(buffer, sizeof(buffer) - 1);
    if (result <= 0) {
      break;
    }
    total_received += result;
    parseBBC(buffer, result);
  }

  socket.close();
  showonLCD(state);
}

using json = nlohmann::json;

int main() {

  Buzzer.period(1.0 / 2000);

  lcd.init();
  lcd.setRGB(255, 255, 255);
  lcd.display();
  lcd.printf("Connecting...");

  button1.fall(&call_back1);
  button2.fall(&call_back2);
  button3.fall(nullptr);
  button4.fall(nullptr);

  NetworkInterface *network = nullptr;

  do {
    network = NetworkInterface::get_default_instance();
    ThisThread::sleep_for(1000ms);
  } while (network == nullptr);

  nsapi_size_or_error_t result;

  do {
    printf("Connecting...\n");
    result = network->connect();

    if (result != NSAPI_ERROR_OK) {
      printf("Failed to connect: %d\n", result);
    } else if (result == NSAPI_ERROR_OK) {
      printf("Connection succcessfull!\n");
    }
  } while (result != NSAPI_ERROR_OK);

  SocketAddress address;
  do {
    result = network->get_ip_address(&address);
  } while (result != NSAPI_ERROR_OK);

  printf("Connected to WLAN and got IP address %s\n", address.get_ip_address());

  TLSSocket *socket = new TLSSocket;
  socket->set_timeout(500);
  result = socket->open(network);

  ////////////////Get ipgeolocation/////////////////////
  const char host[] = "api.ipgeolocation.io";
  result = network->gethostbyname(host, &address);
  printf("IP address of server %s is %s\n", host, address.get_ip_address());

  // Set server TCP port number
  address.set_port(443);

  result = socket->set_root_ca_cert(CERTIFICATE);

  socket->set_hostname(host);
  // Connect to server at the given address
  result = socket->connect(address);

  const char http_request[] =
      "GET /timezone?apiKey=780ed75a587b4381930f08b992d77d50 HTTP/1.1\r\n"
      "Host: api.ipgeolocation.io\r\n"
      "Connection: close\r\n"
      "\r\n";

  nsapi_size_t bytes_to_send = strlen(http_request);
  nsapi_size_or_error_t bytes_sent = 0;

  while (bytes_to_send) {
    bytes_sent = socket->send(http_request + bytes_sent, bytes_to_send);

    if (bytes_sent < 0) {
      break;
    } else {
      printf("Sent %d bytes\n", bytes_sent);
    }
    bytes_to_send -= bytes_sent;
  }
  printf("Messege sent\n");

  // Getting the response
  char chunk[100] = {0};
  static char buffer[2000 + 1]; // Makes room for \0;
  nsapi_size_t remaining_bytes = 2000;
  nsapi_size_or_error_t recieved_bytes = 0;

  memset(buffer, 0, sizeof(buffer));

  while (remaining_bytes > 0) {
    nsapi_size_or_error_t result =
        socket->recv(buffer + recieved_bytes, remaining_bytes);

    // for (int i = 0; i<100; i++) {
    // printf("%c", buffer[i]);
    // chunk[i]=0;
    //}

    if (result < 0) {
      break;
    }

    if (result == 0) {
      break;
    }
    recieved_bytes += result;
    remaining_bytes -= result;
  }

  char headers[200] = {0};
  nsapi_size_or_error_t headers_received =
      socket->recv(headers, sizeof(headers));
  socket->close();
  delete socket;

  // Find the start and end of the JSON data.
  // If the JSON response is an array you need to replace this with [ and ]
  char *json_begin = strchr(buffer, '{');
  char *json_end = strrchr(buffer, '}');

  json_end[1] = 0;

  printf("\nJSON response:\n%s\n", json_begin);

  json document = json::parse(json_begin);

  double unix_time;
  std::string latitude = json_var(document, "latitude");
  std::string longitude = json_var(document, "longitude");
  std::string city = json_var(document, "city");
  int dst;

  document["timezone_offset_with_dst"].get_to(dst);

  document["date_time_unix"].get_to(unix_time);

  int time_offset = 3600 * dst;

  set_time(unix_time + time_offset);
  ////////////////Get ipgeolocation/////////////////////
  ////////////////Weather/////////////////////

  SocketAddress address2;

  const char weather_host[] = "api.weatherapi.com";
  result = network->gethostbyname(weather_host, &address2);

  printf("IP address of server %s is %s\n", weather_host,
         address2.get_ip_address());

  address2.set_port(443);
  // result = socket2.set_root_ca_cert(WEATHER_CERTIFICATE);
  // socket2.set_hostname(weather_host);

  TLSSocket *sock = new TLSSocket;
  result = sock->open(network);
  sock->set_timeout(3000);
  result = sock->set_root_ca_cert(WEATHER_CERTIFICATE);

  sock->set_hostname(weather_host);

  result = sock->connect(address2);// Connect to server at the given address

  static char weather_request[300];

  snprintf(
      weather_request, 300,
      "GET /v1/current.json?key=a36088dcd4984a17a66183727241405&q=%s&aqi=no "
      "HTTP/1.1\r\n"
      "Host: api.weatherapi.com\r\n"
      "Connection: close\r\n"
      "\r\n",
      city.c_str());

  nsapi_size_t bytes_to_send2 = strlen(weather_request);
  nsapi_size_or_error_t bytes_sent2 = 0;

  while (bytes_to_send2) {
    bytes_sent2 = sock->send(weather_request + bytes_sent2, bytes_to_send2);

    if (bytes_sent2 < 0) {
      break;
    } else {
      printf("Sent %d bytes\n", bytes_sent2);
    }
    bytes_to_send2 -= bytes_sent2;
  }
  printf("Messege sent\n");

  // Getting the response
  char chunk2[100] = {0};
  static char buffer2[2000 + 1]; // Makes room for \0;
  nsapi_size_t remaining_bytes2 = 2000;
  nsapi_size_or_error_t recieved_bytes2 = 0;

  memset(buffer2, 0, sizeof(buffer2));

  while (remaining_bytes2 > 0) {
    nsapi_size_or_error_t result =
        sock->recv(buffer2 + recieved_bytes2, remaining_bytes2);
    if (result < 0) {
      break;
    }

    if (result == 0) {
      break;
    }
    recieved_bytes2 += result;
    remaining_bytes2 -= result;
  }

  char headers2[200] = {0};
  nsapi_size_or_error_t headers_received2 =
      sock->recv(headers2, sizeof(headers2));

  char *json_begin2 = strchr(buffer2, '{');
  char *json_end2 = strrchr(buffer2, '}');

  json_end2[1] = 0;

  printf("\nJSON response:\n%s\n", json_begin2);

  json document2 = json::parse(json_begin2);

  std::string weather_forecast;

  if (document2["current"]["condition"]["text"].is_string()) {
    weather_forecast =
        document2["current"]["condition"]["text"].get<std::string>();
  }

  float temperature;
  document2["current"]["temp_c"].get_to(temperature);

  sock->close();
  delete sock;
  ////////////////Weather/////////////////////

  // 2 seconds screens
  ThisThread::sleep_for(2s);
  // Unix time
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.printf("Unix Eepoch time:\n");
  lcd.setCursor(0, 1);
  lcd.printf("%f", unix_time);
  ThisThread::sleep_for(2s);
  // Coordinates
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.printf("Lat: %s", latitude.c_str());
  lcd.setCursor(0, 1);
  lcd.printf("Long: %s", longitude.c_str());
  ThisThread::sleep_for(2s);
  // Current City
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.printf("City:");
  lcd.setCursor(0, 1);
  lcd.printf("%s", city.c_str());
  ThisThread::sleep_for(2s);

  while (true) {

    time_t seconds = time(NULL);

    char buffer[32];
    strftime(buffer, 32, "%a %d %b %H:%M", localtime(&seconds));

    struct tm *local_time = localtime(&seconds);

    // Extract day, month, and year
    int day = local_time->tm_mday;
    int month = local_time->tm_mon + 1;    // Month is zero-based, so add 1
    int year = local_time->tm_year + 1900; // Year starts from 1900
    int current_hour = local_time->tm_hour;
    int current_min = local_time->tm_min;

    // Get the day of the week in text form
    char day_of_week[20];
    strftime(day_of_week, sizeof(day_of_week), "%A", local_time);

    if (current_hour == a_hours && current_min == a_minutes && alarm_set) {
      if (!muted && !snooze) {
        Buzzer.write(0.5);
        alarm_active = true;
        if (alarm_active && !auto_snooze) {
          timer.attach(&mute_buzzer, auto_mute_counter);
          auto_snooze = true;
        }
      }
    }
    if (alarm_active && muted) {
      Buzzer.write(0);
      alarm_active = false;
    }

    if (button5 == 0) {
      alarm_set = !alarm_set;
    }

    // Set alarm
    if (b2_pressed) {

      button3.fall(&call_back3);
      button4.fall(&call_back4);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.printf("Setting Alarm");
      lcd.setCursor(0, 1);
      if (a_hours < 10 && a_minutes < 10) {
        lcd.printf("0%i:0%i", a_hours, a_minutes);
      } else if (a_hours >= 10 && a_minutes < 10) {
        lcd.printf("%i:0%i", a_hours, a_minutes);
      } else if (a_hours < 10 && a_minutes >= 10) {
        lcd.printf("0%i:%i", a_hours, a_minutes);
      } else if (a_hours >= 10 && a_minutes >= 10) {
        lcd.printf("%i:%i", a_hours, a_minutes);
      }
      button2.fall(&confirm_alarm);

    } else {
      // Week screen
      if (state == 0) {
        lcd.clear();
        lcd.setCursor(0, 0);
        // lcd.printf(day_of_week);
        lcd.printf("%s", buffer);

        // alarm screen
        if (alarm_set) {
          lcd.setCursor(0, 1);
          if (a_hours < 10 && a_minutes < 10) {
            lcd.printf("Alarm: 0%i:0%i", a_hours, a_minutes);
          } else if (a_hours >= 10 && a_minutes < 10) {
            lcd.printf("Alarm: %i:0%i", a_hours, a_minutes);
          } else if (a_hours < 10 && a_minutes >= 10) {
            lcd.printf("Alarm: 0%i:%i", a_hours, a_minutes);
          } else if (a_hours >= 10 && a_minutes >= 10) {
            lcd.printf("Alarm: %i:%i", a_hours, a_minutes);
          }
          if (alarm_active) {
            lcd.printf(" (A)");
          } else if (snooze) {
            lcd.printf(" (S)");
          }
        }
      }

      // Temp screen
      if (state == 1) {
        float temp = 0;
        float hum = 0;
        sensor.get_temperature(&temp);
        sensor.get_humidity(&hum);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.printf("Temp: %.1fC", temp);
        lcd.setCursor(0, 1);
        lcd.printf("Humidity: %.2f%", hum);
      }

      // Weather forecast
      if (state == 2) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.printf("%s", weather_forecast.c_str());
        lcd.setCursor(0, 1);
        lcd.printf("%.1fC", temperature);
      }

      // News
      if (state == 3) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.printf("Fetching News");
        lcd.setCursor(0, 1);
        lcd.printf("One Moment...");
        hentefeed(network, "https://feeds.bbci.co.uk/news/world/rss.xml",
                  state);
        state = 0;
      }
    }
    led = !led;
    ThisThread::sleep_for(BLINKING_RATE);
  }
}
