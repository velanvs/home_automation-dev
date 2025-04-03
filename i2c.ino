#include <Wire.h>

#include <PubSubClient.h>


#include <WiFi.h>
#include <WiFiAP.h>
#include <WiFiClient.h>
#include <WiFiGeneric.h>
#include <WiFiMulti.h>
#include <WiFiSTA.h>
#include <WiFiScan.h>
#include <WiFiServer.h>
#include <WiFiType.h>
#include <WiFiUdp.h>

/*!
 * @file  i2c.ino
 * @brief  Control the voice recognition module via I2C
 * @n  Get the recognized command ID and play the corresponding reply audio according to the ID;
 * @n  Get and set the wake-up state duration, set mute mode, set volume, and enter the wake-up state
 * @copyright  Copyright (c) 2010 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @licence  The MIT License (MIT)
 * @author  [qsjhyy](yihuan.huang@dfrobot.com)
 * @version  V1.0
 * @date  2022-12-30
 * @url  https://github.com/DFRobot/DFRobot_DF2301Q
 */
#include "DFRobot_DF2301Q.h"


#define MAX_NUM_TOPICS 8


#define STATUS_LED 2




#define HALL_LIGHT      32
#define AC_BUTTON       33
#define KITCHEN_LIGHT     26
#define BATH_ROOM_LIGHT   27
#define PUMP_MOTOR_INT1     16
#define PUMP_MOTOR_INT2     17
#define LIVING_ROOM_FAN_INT1  18
#define LIVING_ROOM_FAN_INT2  19

#define HALL_LIGHT_ON 5
#define HALL_LIGHT_OFF 6

#define KITCHIEN_LIGHT_ON 7
#define KITCHIEN_LIGHT_OFF 8

#define BATH_ROOM_LIGHT_ON 10
#define BATH_ROOM_LIGHT_OFF 9

#define FAN_ON   75
#define FAN_OFF  76

#define AC_ON   124
#define AC_OFF  125

#define PUMP_ON 11
#define PUMP_OFF 12

#define POWER_SAVE_MODE_ON 13
#define POWER_SAVE_MODE_OFF 14

#define ALL_LIGHT_ON 15
#define ALL_LIGHT_OFF 16

#define WIFI_CONNECTED       1  //1 sec blink rate
#define WIFI_CONNECTION_FAILED 2  //100 ms blink 
#define MQTT_CONNECTION_FAILED  3  // 500 ms

#define WIFI_MQTT_CONNECTED  4 // 1 sec on - 100 ms off 

  uint8_t led_counter_timer = 20;  // 20 * 50ms =1 sec 

const char* ssid = "velanmx";
const char* password = "12345678";

const char* broker = "www.maqiatto.com";
const int port = 1883;
const char* client_id = "esp32_sha";

const char* mqtt_username = "velanv5144@gmail.com"; // Replace with your username
const char* mqtt_password = "12345678"; // Replace with your password

const char* topic1 = "velanv5144@gmail.com/sha/hall_light";
const char* topic2 = "velanv5144@gmail.com/sha/ac";
const char* topic3 = "velanv5144@gmail.com/sha/kitchen_light";
const char* topic4 = "velanv5144@gmail.com/sha/bath_room_light";
const char* topic5 = "velanv5144@gmail.com/sha/pump_motor";
const char* topic6 = "velanv5144@gmail.com/sha/living_room_fan";
const char* topic7 = "velanv5144@gmail.com/sha/all_light_control";
const char* topic8 = "velanv5144@gmail.com/sha/power_save_mode";


const char* topic_pub1 = "velanv5144@gmail.com/sha/pub/hall_light";
const char* topic_pub2 =  "velanv5144@gmail.com/sha/pub/ac";
const char* topic_pub3 = "velanv5144@gmail.com/sha/pub/kitchen_light";
const char* topic_pub4 = "velanv5144@gmail.com/sha/pub/bath_room_light";
const char* topic_pub5 = "velanv5144@gmail.com/sha/pub/pump_motor";
const char* topic_pub6 = "velanv5144@gmail.com/sha/pub/living_room_fan";
const char* topic_pub7 = "velanv5144@gmail.com/sha/pub/all_light_control";
const char* topic_pub8 = "velanv5144@gmail.com/sha/pub/power_save_mode";


typedef enum {
  HALL_LIGHT_ID =1,
  AC_ID,
  KITCHEN_LIGHT_ID,
  BATH_ROOM_LIGHT_ID,
  PUMP_ID,
  FAN_ID,
  ALL_LIGHT_ID,
  POWER_SAVE_ID
}light_topics;


const char *ptr[MAX_NUM_TOPICS] = { topic1,topic2, topic3 ,topic4 ,topic5, topic6, topic7 , topic8};
//I2C communication
DFRobot_DF2301Q_I2C DF2301Q;

WiFiClient espClient;
PubSubClient client(espClient);

long lastReconnectAttempt = 0; // Tracks the last reconnect time

//Queue for the controling the light 
xQueueHandle appQueue={NULL};

typedef struct {

  int id;
  int data;
}light_cmd_queue;
light_cmd_queue light_signal_send;

void mqtt_callback(char* topic, byte* payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) 
  {
    Serial.print((char)payload[i]);
  }
    for(int i=1; i<=MAX_NUM_TOPICS;i++)
    {
      if(strcmp(topic,ptr[i-1]) == 0)
      {
        light_signal_send.id =i;
        char payloadStr[length + 1]; // +1 for null terminator
        memcpy(payloadStr, payload, length);
        payloadStr[length] = '\0'; // Null-terminate the string
        light_signal_send.data = atoi(payloadStr);

        if(xQueueSend(appQueue,&light_signal_send, 1000) != pdPASS)
        {
          printf("send queue failed\n\r");
        }
        break;
      }
    }
  Serial.println();
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(client_id,mqtt_username,mqtt_password)) {
      Serial.println("connected");
    client.subscribe(topic1);
    client.subscribe(topic2);
    client.subscribe(topic3);
    client.subscribe(topic4);
    client.subscribe(topic5);
    client.subscribe(topic6);
    client.subscribe(topic7);
    client.subscribe(topic8);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000); // Wait before retrying
    }
  }
}

void mqttTask(void *pvParameters) {
  long lastReconnectAttempt = 0;
  while (1) 
  {
    if (!client.connected()) 
    {
      long now = millis();
      if (now - lastReconnectAttempt > 5000) 
      {
        lastReconnectAttempt = now;
        Serial.print("Attempting MQTT connection...");
        if (client.connect("esp32_sha", mqtt_username, mqtt_password)) {
          Serial.println("connected");
          client.subscribe(topic1 );
          client.subscribe(topic2 );
          client.subscribe(topic3 );
          client.subscribe(topic4 );
          client.subscribe(topic5 );
          client.subscribe(topic6 );
          client.subscribe(topic7 );
          client.subscribe(topic8 );
        } 
        else 
        {
          Serial.print("failed, rc=");
          Serial.print(client.state());
          Serial.println(" try again in 5 seconds");
          delay(5000);
        }
      }
    } 
    else 
    {
      client.loop();
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void voiceTask(void *pvParameters) 
{
  light_cmd_queue send_light_control;
  while (1)
  {
    uint8_t CMDID = DF2301Q.getCMDID();
    if (0 != CMDID) 
    {
      Serial.print("CMDID = ");
      Serial.println(CMDID);
      switch(CMDID)
      {
        case HALL_LIGHT_ON:
        {
          send_light_control.id = HALL_LIGHT_ID;
          send_light_control.data = 1;

          if(xQueueSend(appQueue,&send_light_control, 1000) != pdPASS)
          {
            printf("send queue failed\n\r");
          }
          if(client.connected())
          {
            client.publish(topic_pub1, "1");
          }
        }
        break; 
        case HALL_LIGHT_OFF:
        { 
          send_light_control.id = HALL_LIGHT_ID;
          send_light_control.data = 0;

          if(xQueueSend(appQueue,&send_light_control, 1000) != pdPASS)
          {
            printf("send queue failed\n\r");
          }
    
          if(client.connected())
          {
            client.publish(topic_pub1, "0");
          }
        }
        break; 
        case AC_ON:
        {
            send_light_control.id = AC_ID;
            send_light_control.data = 1;

            if(xQueueSend(appQueue,&send_light_control, 1000) != pdPASS)
            {
              printf("send queue failed\n\r");
            }
          if(client.connected())
          {
            client.publish(topic_pub2, "1");
          }
        }
        break; 
        case AC_OFF:
        {
          send_light_control.id = AC_ID;
          send_light_control.data = 0;

          if(xQueueSend(appQueue,&send_light_control, 1000) != pdPASS)
          {
            printf("send queue failed\n\r");
          }
          if(client.connected())
          {
            client.publish(topic_pub2, "0");
          }
        }
        break;         
        case KITCHIEN_LIGHT_ON: 
        {
          send_light_control.id = KITCHEN_LIGHT_ID;
          send_light_control.data = 1;

          if(xQueueSend(appQueue,&send_light_control, 1000) != pdPASS)
          {
            printf("send queue failed\n\r");
          }
          if(client.connected())
          {
            client.publish(topic_pub3, "1");
          }
        }
        break; 
        case KITCHIEN_LIGHT_OFF:
        {
          send_light_control.id = KITCHEN_LIGHT_ID;
          send_light_control.data = 0;

          if(xQueueSend(appQueue,&send_light_control, 1000) != pdPASS)
          {
            printf("send queue failed\n\r");
          }

          if(client.connected())
          {
            client.publish(topic_pub3, "0");
          }

        }
        break; 
        case BATH_ROOM_LIGHT_ON:
        {
          send_light_control.id =BATH_ROOM_LIGHT_ID;
          send_light_control.data = 1;

          if(xQueueSend(appQueue,&send_light_control, 1000) != pdPASS)
          {
            printf("send queue failed\n\r");
          }
          if(client.connected())
          {
            client.publish(topic_pub4, "1");
          }
        }
        break; 
        case BATH_ROOM_LIGHT_OFF:
        {
          send_light_control.id = BATH_ROOM_LIGHT_ID;
          send_light_control.data = 0;

          if(xQueueSend(appQueue,&send_light_control, 1000) != pdPASS)
          {
            printf("send queue failed\n\r");
          }
          if(client.connected())
          {
            client.publish(topic_pub4, "0");
          }
        }
        break; 
        case PUMP_ON:
        {
          send_light_control.id = PUMP_ID;
          send_light_control.data = 1;
          if(xQueueSend(appQueue,&send_light_control, 1000) != pdPASS)
          {
            printf("send queue failed\n\r");
          }
          if(client.connected())
          {
            client.publish(topic_pub5, "1");
          } 
        }       
        break;
        case PUMP_OFF:
        {
          send_light_control.id = PUMP_ID;
          send_light_control.data = 0;

          if(xQueueSend(appQueue,&send_light_control, 1000) != pdPASS)
          {
            printf("send queue failed\n\r");
          }
          if(client.connected())
          {
            client.publish(topic_pub5, "0");
          }        
        }
        break;
        case FAN_ON:
        {
          send_light_control.id = FAN_ID;
          send_light_control.data = 1;

          if(xQueueSend(appQueue,&send_light_control, 1000) != pdPASS)
          {
            printf("send queue failed\n\r");
          }
          if(client.connected())
          {
            client.publish(topic_pub6, "1");
          }        
        }
        break;
        case FAN_OFF:
        {
          send_light_control.id = FAN_ID;
          send_light_control.data = 0;
          if(xQueueSend(appQueue,&send_light_control, 1000) != pdPASS)
          {
           printf("send queue failed\n\r");
          }
          if(client.connected())
          {
            client.publish(topic_pub6, "0");
          }  
        }   
        break;
        case ALL_LIGHT_ON:
        {
          send_light_control.id = ALL_LIGHT_ID;
          send_light_control.data = 1;

          if(xQueueSend(appQueue,&send_light_control, 1000) != pdPASS)
          {
            printf("send queue failed\n\r");
          }
          if(client.connected())
          {
            client.publish(topic_pub7, "1");
          }
        }
        break;
        case ALL_LIGHT_OFF :
        {
          send_light_control.id = ALL_LIGHT_ID;
          send_light_control.data = 0;

          if(xQueueSend(appQueue,&send_light_control, 1000) != pdPASS)
          {
            printf("send queue failed\n\r");
          }
          if(client.connected())
          {
            client.publish(topic_pub7, "0");
          }
        }
        break;
        case POWER_SAVE_MODE_ON:
        {
          send_light_control.id = POWER_SAVE_ID;
          send_light_control.data = 1;
          if(xQueueSend(appQueue,&send_light_control, 1000) != pdPASS)
          {
            printf("send queue failed\n\r");
          }
          if(client.connected())
          {
            client.publish(topic_pub8, "1");
          }
        }
        break;
        case POWER_SAVE_MODE_OFF:
        {
          send_light_control.id = POWER_SAVE_ID;
          send_light_control.data = 0;

          if(xQueueSend(appQueue,&send_light_control, 1000) != pdPASS)
          {
            printf("send queue failed\n\r");
          }
          if(client.connected())
          {
            client.publish(topic_pub8, "0");
          }
        }
        break;

        default:
        break;
      }
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}
void appTask(void *pvParameters)
{
  light_cmd_queue light_signal;
  
  while(1)
  {
    if(xQueueReceive(appQueue,&light_signal,portMAX_DELAY) == pdTRUE)
    {
      printf("queue received id =%d data =%d\n\r",light_signal.id , light_signal.data);
      switch(light_signal.id)
      {
        case HALL_LIGHT_ID:
          if (light_signal.data == 1)
          {
            printf(" Hall room light On");
            digitalWrite( HALL_LIGHT,HIGH);
          }
          else if (light_signal.data == 0)
          {
            printf(" Hall room light Off");
            digitalWrite( HALL_LIGHT,LOW);
          }
          else;
        break;
        case AC_ID:
          if (light_signal.data == true)
          {
            printf(" AC On");
            digitalWrite(AC_BUTTON ,HIGH);
          }
          else if (light_signal.data == false)
          {
            printf(" AC Off");
            digitalWrite(AC_BUTTON ,LOW);
          }
          else;
        break;
        case KITCHEN_LIGHT_ID:
          if (light_signal.data == true)
          {
            printf(" Kitchen room light On");
            digitalWrite(KITCHEN_LIGHT ,HIGH);
          
          }
          else if (light_signal.data == false)
          {
            printf(" Kitchen room light Off");
            digitalWrite( KITCHEN_LIGHT,LOW);
          }
          else;
        break;

        case BATH_ROOM_LIGHT_ID:
          if (light_signal.data == true)
          {
            printf(" Bath  room light On");
             digitalWrite(BATH_ROOM_LIGHT ,HIGH);
          
          }
          else if (light_signal.data == false)
          {
            printf(" Bath room light Off");
            digitalWrite(BATH_ROOM_LIGHT ,LOW);
          }
          else;
        break;
        case PUMP_ID:
          if (light_signal.data == true)
          {
            printf(" Pump Motor On");
             digitalWrite(PUMP_MOTOR_INT1 ,HIGH);
              digitalWrite(PUMP_MOTOR_INT2,LOW);
          
          }
          else if (light_signal.data == false)
          {
            printf(" Pump Motor Off");
            digitalWrite(PUMP_MOTOR_INT1 ,LOW);
            digitalWrite(PUMP_MOTOR_INT2 ,LOW);
          }
          else;
        case FAN_ID:
          if (light_signal.data == true)
          {
            printf(" fan On");
              digitalWrite(LIVING_ROOM_FAN_INT1 ,HIGH);
              digitalWrite(LIVING_ROOM_FAN_INT2,LOW);
          
          }
          else if (light_signal.data == false)
          {
            printf(" fan Off");

              digitalWrite(LIVING_ROOM_FAN_INT1 ,LOW);
              digitalWrite(LIVING_ROOM_FAN_INT2 ,LOW);
          }
          else;
        break;

        case ALL_LIGHT_ID:
          if (light_signal.data == true)
          {
            printf(" All light On");
            digitalWrite((HALL_LIGHT | AC_BUTTON | KITCHEN_LIGHT | BATH_ROOM_LIGHT),HIGH );
          
          }
          else if (light_signal.data == false)
          {
            printf(" All light Off");
            digitalWrite((HALL_LIGHT | AC_BUTTON | KITCHEN_LIGHT | BATH_ROOM_LIGHT),LOW);
          }
          else;
        break;
        case POWER_SAVE_ID:
          if (light_signal.data == true)
          {
            printf(" Power Save Mode On");
            digitalWrite((HALL_LIGHT | AC_BUTTON | KITCHEN_LIGHT |\
             BATH_ROOM_LIGHT | PUMP_MOTOR_INT1 | PUMP_MOTOR_INT2 |LIVING_ROOM_FAN_INT1 |LIVING_ROOM_FAN_INT2 ), LOW);
          
          }
          else if (light_signal.data == false)
          {
            printf(" Power Save Mode Off");
          }
          else;
        break;

        default:
        break; 
      }
    }
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }

}
// void ledIndication( void *pvParameters)
// {
//  int led_state=0;
//   while(1)
//   {
//     switch(led_state)
//     {
//       case WIFI_CONNECTED:
//         if(led_counter_timer !=0x00)
//         {
          
//          // --led_delay_count;
//         }
//         else
//         {
//           digitalWrite(STATUS_LED,HIGH);

//           //led_state=WIFI_

//         }

//       break;
//     }
//     vTaskDelay(50 / portTICK_PERIOD_MS);
//   }
// }
void setup()
{
  
  Serial.begin(115200);

  pinMode(STATUS_LED,OUTPUT);

  pinMode(HALL_LIGHT,OUTPUT);

  pinMode(AC_BUTTON,OUTPUT);

  pinMode(BATH_ROOM_LIGHT,OUTPUT);

  pinMode(KITCHEN_LIGHT,OUTPUT);

  pinMode(PUMP_MOTOR_INT1,OUTPUT);

  pinMode(PUMP_MOTOR_INT2,OUTPUT);

  pinMode(LIVING_ROOM_FAN_INT1,OUTPUT);

  pinMode(LIVING_ROOM_FAN_INT2,OUTPUT);
 

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  client.setServer(broker, port);
  client.setCallback(mqtt_callback);

  appQueue=xQueueCreate(10,sizeof(light_cmd_queue));

  xTaskCreate(mqttTask, "MQTTTask", 4096, NULL, 5, NULL);
  xTaskCreate(voiceTask, "VoiceTask", 4096, NULL, 3, NULL);
  xTaskCreate(appTask, "AppTask", 4096, NULL, 5 ,NULL);

 // xTaskCreate(ledIndication, "LEDTask", 4096, NULL, 6 ,NULL);

  // Init the sensor
  while( !( DF2301Q.begin() ) ){

    Serial.println("Communication with device failed, please check connection");

    delay(3000);
    
  }
  Serial.println("Begin ok!");

  /**
   * @brief Set voice volume
   * @param voc - Volume value(1~7)
   */
  DF2301Q.setVolume(6);

  /**
   * @brief Set mute mode
   * @param mode - Mute mode; set value 1: mute, 0: unmute
   */
  DF2301Q.setMuteMode(0);

  /**
   * @brief Set wake-up duration
   * @param wakeTime - Wake-up duration (0-255)
   */
  DF2301Q.setWakeTime(15);

  /**
   * @brief Get wake-up duration
   * @return The currently-set wake-up period
   */
  uint8_t wakeTime = 0;
  wakeTime = DF2301Q.getWakeTime();
  Serial.print("wakeTime = ");
  Serial.println(wakeTime);

  /**
   * @brief Play the corresponding reply audio according to the command word ID
   * @param CMDID - Command word ID
   * @note Can enter wake-up state through ID-1 in I2C mode
   */
  // DF2301Q.playByCMDID(1);   // Wake-up command
  DF2301Q.playByCMDID(23);   // Common word ID

}

void loop()
{
  vTaskDelete(NULL); // Idle task, delete it.
}
