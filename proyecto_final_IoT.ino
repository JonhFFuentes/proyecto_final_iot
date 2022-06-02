#include <ThingSpeak.h>
#include <ESP32_MailClient.h>
#include <DHT.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <PubSubClient.h>


#define DHTPIN 4
#define DHTTYPE DHT11

//**************
//**** MQTT CONFIG *****
//**************
const char *mqtt_server = "node02.myqtthub.com";
const int mqtt_port = 1883;
const char *mqtt_user = "1";
const char *mqtt_pass = "11";
const char *root_topic_subscribe = "InfoHogar/in";
const char *root_topic_publish = "InfoHogar/out";

const char *root_topic_subscribe_door = "InfoHogar/puerta";
const char *root_topic_subscribe_modifyTemp = "InfoHogar/modifcartemperatura";
const char *root_topic_publish_data = "InfoHogar/Datos";
const char *root_topic_publish_temp = "InfoHogar/temperatura";
const char *root_topic_publish_humedity = "InfoHogar/humedad";

unsigned long channelID = 1755749;                //ID de vuestro canal.
const char* WriteAPIKey = "T8R9QSDZ1YD37337";     //Write API Key de vuestro canal.

//Config mail
#define emailSenderAccount    "electivaiotunisangil@gmail.com"    
#define emailSenderPassword   "ElectivaIOT20"
#define emailRecipient        "jonhfuentes4@unisangil.edu.co"
#define smtpServer            "smtp.gmail.com"
#define smtpServerPort        465

String emailSubject = "MENSAJE ENVIADO DESDE ESP 32";
String emailBodyMessage = "<div style=\"color:#000000;\"><p>- DATOS ESP 32</p></div>";

//**************
//**** WIFICONFIG ******
//**************
const char* ssid ="Claro_65748E" ;
const char* password="H6C5R6H2D6F7";

//**************
//**** GLOBAL CONFIG *****
//**************
float temperature=0;
float humidity=0;
int stateRele=0;
int stateServo=0;
static const int servoPin = 23;
const int rele = 22;

String topic="";
String body="";
int tempModify=34;
int banderaControllerTempMail = 0;
float lastTemperature;

const unsigned long publishTempAll = 25000;
unsigned long lastPublishTempAll;

const unsigned long publishHumedityAll = 25000;
unsigned long lastPublishHumedityAll;


//objects
DHT dht(DHTPIN, DHTTYPE);
Servo myservo;
SMTPData smtpData;



//**************
//**** GLOBALES   ******
//**************
WiFiClient espClient;
PubSubClient client(espClient);
char msg[25];
long count=0;

//************************
//** F U N C I O N E S ***
//************************
void callback(char* topic, byte* payload, unsigned int length);
void reconnect();
void setup_wifi();
// Callback function to get the Email sending status
void sendCallback(SendStatus info);

void read_sensor_data(void * parameter) {
   for (;;) {
    readDataSensor();
    readModifyTemp();
   }
}

void readDataSensor(){
  humidity = dht.readHumidity();
  lastTemperature = dht.readTemperature();
  if(!isnan(lastTemperature)){
    temperature = lastTemperature;
    }
  Serial.print(F("Humedad: "));
  Serial.print(humidity);
  Serial.print("%");
  Serial.print(F("  Temperatura: "));
  Serial.print(temperature);
  Serial.println(F("°C "));
  delay(10000);
  }

void readTopicDoor(){
  Serial.println(topic);
  if(topic!=""){
    String readTopic = topic.substring(10,16);
    Serial.println(readTopic);
    if(readTopic=="puerta"){
      if(body=="abrir"){
        Serial.println("readTopic -> ok");
        Serial.println(body);
        myservo.write(180);
        emailSubject = "ESP32 ALERTA - PUERTA ABIERTA";
        emailBodyMessage = "<div style=\"color:#2f4468;\"><h1>Hola esto es una notificacion!</h1><p>- La puerta a sido abierta</p></div>";
        ConfigSendMailTo();
        topic="";
        stateServo = 1;
        }
      if(body=="cerrar"){
        Serial.println("readTopic -> ok");
        Serial.println(body);
        myservo.write(1);
        topic="";
        stateServo = 0;
        }
       if(body=="enviarCorreo"){
        Serial.println("enviarCorreo -> ok");
        Serial.println(body);
        ConfigSendMailTo();
        }
      }
    }
  }

 void readModifyTemp(){
  Serial.println(topic);
  if(topic!=""){
    String readTopic = topic.substring(10,29);
    Serial.println(readTopic);
    if(readTopic=="modifcartemperatura"){
      if(body.toInt()>=0){
        Serial.println("readModifyTemp -> ok");
        Serial.println(body.toInt());
        tempModify = body.toInt();
        topic="";
        }
      }
    }
  }

 void controllerTemp(){
  if(temperature>tempModify){
    Serial.println("Encendido Reley -> ok");
    digitalWrite(rele,HIGH);
    emailSubject = "ESP32 ALERTA - VENTILADOR ENCENDIDO";
    emailBodyMessage = "<div style=\"color:#2f4468;\"><h1>Hola señor usuario, esto es una notificacion!</h1><p>- La temperatura a subido, obligando a encender el ventilador</p></div>";
    if(banderaControllerTempMail == 0){
      ConfigSendMailTo();
      banderaControllerTempMail = 1; 
      }
    }else{
      Serial.println("Apagado Reley -> ok");
      digitalWrite(rele,LOW);
      banderaControllerTempMail = 0;
      }
  }

  // Callback function to get the Email sending status
void sendCallback(SendStatus msg) {
  // Print the current status
  Serial.println(msg.info());

  // Do something when complete
  if (msg.success()) {
    Serial.println("----------------");
  }
}
  


void setup_task() {
  xTaskCreate(
    read_sensor_data,    
    "Read sensor data",  
    1000,            
    NULL,            
    1,               
    NULL            
  );
}

void initServo(){
  // Allow allocation of all timers for servo library
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  // Set servo PWM frequency to 50Hz
  myservo.setPeriodHertz(50);
  // Attach to servo and define minimum and maximum positions
  // Modify as required
  myservo.attach(servoPin,500, 2400);
  }


void setup() {
  pinMode(rele,OUTPUT);
  initServo();
  dht.begin();
  Serial.begin(115200);
  setup_wifi();
  setup_task();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  ThingSpeak.begin(espClient);
}

void loop() {    
  if (!client.connected()) {
    reconnect();
  }

  if (client.connected()){
    dataPublish();
    tempPublish();
    humedityPublish();
    
    delay(25000);
  }
  client.loop();
  readTopicDoor();
  controllerTemp();

  ThingSpeak.writeFields(channelID,WriteAPIKey);
  Serial.println("Datos enviados a ThingSpeak!");
  delay(14000);
}



void tempPublish(){
  //My timer
  unsigned long topLoop = millis();
  // this is setInterval
  if (topLoop - lastPublishHumedityAll >= publishHumedityAll) {
    lastPublishHumedityAll = topLoop;
    String str = "La Temperatura es -> " + String(temperature)+ "°C";
    str.toCharArray(msg,30);
    client.publish(root_topic_publish_temp,msg);
    ThingSpeak.setField (1,temperature);
    
    delay(200);
    delay(25000);
    
    }
}

void humedityPublish(){
  //My timer
  unsigned long topLoop2 = millis();
  // this is setInterval
  if (topLoop2 - lastPublishTempAll >= publishTempAll) {
    lastPublishTempAll = topLoop2;
    String str = "La Humedad es -> "+ String(humidity)+ "%";
    str.toCharArray(msg,30);
    client.publish(root_topic_publish_humedity,msg);
    str="";
    ThingSpeak.setField (2,humidity);
    delay(200);
    delay(25000);
    }
}

void dataPublish(){
  String str = "La Temperatura es -> " + String(temperature)+ "°C";
  str.toCharArray(msg,30);
  client.publish(root_topic_publish_data,msg);
  str="";
  delay(200);
  str = "La Humedad es -> "+ String(humidity)+ "%";
  str.toCharArray(msg,30);
  client.publish(root_topic_publish_data,msg);
  str="";
  delay(200);
  str = "El estado de la puerta es -> "+ String(stateServo);
  str.toCharArray(msg,35);
  client.publish(root_topic_publish_data,msg);
  str="";
  delay(200);
  str = "La temperatura limite es -> "+ String(tempModify);
  str.toCharArray(msg,35);
  client.publish(root_topic_publish_data,msg);
  delay(2000);
  }


//***********
//*    CONEXION WIFI      *
//***********
void setup_wifi(){
  delay(10);
  // Nos conectamos a nuestra red Wifi
  Serial.println();
  Serial.print("Conectando a ssid: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("Conectado a red WiFi!");
  Serial.println("Dirección IP: ");
  Serial.println(WiFi.localIP());
}


//***********
//*    CONEXION MQTT      *
//***********

void reconnect() {

  while (!client.connected()) {
    Serial.print("Intentando conexión Broker...");
    // Creamos un cliente ID
    String clientId = "esp32_j&l";
   
    // Intentamos conectar
    if (client.connect(clientId.c_str(),mqtt_user,mqtt_pass)) {
      Serial.println("Conectado!");
      // Nos suscribimos
      if(client.subscribe(root_topic_subscribe)){
        Serial.println("Suscripcion ok");
      }else{
        Serial.println("fallo Suscripción");
      }
      //topic suscribe door
      if(client.subscribe(root_topic_subscribe_door)){
        Serial.println("Suscripcion topic door ok");
      }else{
        Serial.println("fallo Suscripción to door");
      }
      //topic suscribe modifyTemp
      if(client.subscribe(root_topic_subscribe_modifyTemp)){
        Serial.println("Suscripción topic modifyTemp ok");
      }else{
        Serial.println("fallo Suscripción to modifyTemp");
      }
    } else {
      Serial.print("falló  ");
      Serial.print(client.state());
      Serial.println(" Intenta de nuevo en 5 segundos");
      delay(5000);
    }
  }
}


//***********
//*       CALLBACK        *
//***********

void callback(char* topico, byte* payload, unsigned int length){
  String incoming = "";
  Serial.print("Mensaje recibido desde:");
  Serial.print(topico);
  topic=topico;
  Serial.println("");
  for (int i = 0; i < length; i++) {
    incoming += (char)payload[i];
  }
  incoming.trim();
  body=incoming;
  Serial.println("Mensaje: " + incoming);

}

void ConfigSendMailTo(){
  Serial.println("Preparando para enviar correo");
  Serial.println();  
  // Set the SMTP Server Email host, port, account and password
  smtpData.setLogin(smtpServer, smtpServerPort, emailSenderAccount, emailSenderPassword);

  // For library version 1.2.0 and later which STARTTLS protocol was supported,the STARTTLS will be 
  // enabled automatically when port 587 was used, or enable it manually using setSTARTTLS function.
  //smtpData.setSTARTTLS(true);

  // Set the sender name and Email
  smtpData.setSender("ESP32", emailSenderAccount);

  // Set Email priority or importance High, Normal, Low or 1 to 5 (1 is highest)
  smtpData.setPriority("High");

  // Set the subject
  smtpData.setSubject(emailSubject);

  // Set the message with HTML format
  smtpData.setMessage(emailBodyMessage, true);
  // Set the email message in text format (raw)
  //smtpData.setMessage("Hello World! - Sent from ESP32 board", false);

  // Add recipients, you can add more than one recipient  smtpData.addRecipient(emailRecipient);
  //smtpData.addRecipient("YOUR_OTHER_RECIPIENT_EMAIL_ADDRESS@EXAMPLE.com");

  smtpData.setSendCallback(sendCallback);

  //Start sending Email, can be set callback function to track the status
  if (!MailClient.sendMail(smtpData))
    Serial.println("Error sending Email, " + MailClient.smtpErrorReason());

  //Clear all data from Email object to free memory
  smtpData.empty();
  topic="";
  }
