#include <ESP8266WiFi.h>
#include <WiFiClient.h>


//巴法云服务器地址默认即可
#define TCP_SERVER_ADDR "bemfa.com"
//服务器端口，tcp创客云端口8344
#define TCP_SERVER_PORT "8344"

//********************需要修改的部分*******************//

//WIFI名称，区分大小写，不要写错
#define DEFAULT_STASSID  "put-your-wifi-name-here"
//WIFI密码
#define DEFAULT_STAPSW   "put-your-wifi-password-here"
//用户私钥，可在控制台获取,修改为自己的UID
#define UID  "put-your-bemfa-UID-here"
//主题名字，可在控制台新建
#define TOPIC  "put-your-tcp-topic-here"



//**************************************************//



//最大字节数
#define MAX_PACKETSIZE 512
//设置心跳值30s
#define KEEPALIVEATIME 30*1000

const int ledPin = LED_BUILTIN;        // ESP12S自带的LED引脚
const int pwr    = 4;                  //电源开关
const int rst    = 5;                  //reset

//tcp客户端相关初始化，默认即可
WiFiClient TCPclient;
String TcpClient_Buff = "";
unsigned int TcpClient_BuffIndex = 0;
unsigned long TcpClient_preTick = 0;
unsigned long preHeartTick = 0;//心跳
unsigned long preTCPStartTick = 0;//连接
bool preTCPConnected = false;
unsigned long pressTime = 0; //按钮按下的时刻
#define TAP 0.5  //按钮点按的最大时长，单位s
#define FORCE 5  //按钮长按的最大市场，单位s
bool pressStatus = false;
unsigned long maxtime = TAP * 1000;
int button;


//相关函数初始化
//连接WIFI
void doWiFiTick();
void startSTA();

//TCP初始化连接
void doTCPClientTick();
void startTCPClient();
void sendtoTCPServer(String p);





/*
  *发送数据到TCP服务器
 */
void sendtoTCPServer(String p){
  
  if (!TCPclient.connected()) 
  {
    Serial.println("Client is not readly");
    return;
  }
  TCPclient.print(p);
  Serial.println("[Send to TCPServer]:String");
  Serial.println(p);
}


/*
  *初始化和服务器建立连接
*/
void startTCPClient(){
  if(TCPclient.connect(TCP_SERVER_ADDR, atoi(TCP_SERVER_PORT))){
    Serial.print("\nConnected to server:");
    Serial.printf("%s:%d\r\n",TCP_SERVER_ADDR,atoi(TCP_SERVER_PORT));
    char tcpTemp[128];
    sprintf(tcpTemp,"cmd=1&uid=%s&topic=%s\r\n",UID,TOPIC);

    sendtoTCPServer(tcpTemp);
    preTCPConnected = true;
    preHeartTick = millis();
    TCPclient.setNoDelay(true);
  }
  else{
    Serial.print("Failed connected to server:");
    Serial.println(TCP_SERVER_ADDR);
    TCPclient.stopAll();
    preTCPConnected = false;
  }
  preTCPStartTick = millis();
}

//解析数据
void parseQueryString(const String &input) {
    char buffer[200];
    input.toCharArray(buffer, sizeof(buffer));

    char *save_ptr_outer; // 外层分割状态保存指针
    char *pair = strtok_r(buffer, "&", &save_ptr_outer); // 第一次分割

    while (pair != NULL) {
        char *save_ptr_inner; // 内层分割状态保存指针
        char *key = strtok_r(pair, "=", &save_ptr_inner); // 分割键
        char *value = strtok_r(NULL, "=", &save_ptr_inner); // 分割值

        if (key && value) {
            if (strcmp(key, "cmd") == 0) {
                int cmd = atoi(value);
                Serial.print("CMD: "); Serial.println(cmd);
            } 
            else if (strcmp(key, "uid") == 0) {
                Serial.print("UID: "); Serial.println(value);
            }
            else if (strcmp(key, "topic") == 0) {
                Serial.print("Topic: "); Serial.println(value);
            }
            else if (strcmp(key, "msg") == 0) {
                Serial.print("Message: "); Serial.println(value);
                if(strncmp(value, "power",5) == 0) {
                  maxtime = TAP * 1000;
                  button = pwr;
                  handleLedOn();
                }
                else if(strncmp(value, "force",5) == 0) {
                  maxtime = FORCE * 1000;
                  button = pwr;
                  handleLedOn();
                }
                else if(strncmp(value, "reset",5) == 0) {
                  maxtime = TAP * 1000;
                  button = rst;
                  handleLedOn();
                }
            }
        }
        pair = strtok_r(NULL, "&", &save_ptr_outer); // 继续外层分割
    }
}

// if(strcmp(value, "on") == 0) handleLedOn();
// if(strcmp(value, "off") == 0) handleLedOff();

/*power:点击，持续按下0.5s，松开
  force:强制关机，持续按下5s，松开
  reset:复位，持续按下复位0.5s，松开
*/ 
/*
  *检查数据，发送心跳
*/
void doTCPClientTick(){
 //检查是否断开，断开后重连
   if(WiFi.status() != WL_CONNECTED) return;

  if (!TCPclient.connected()) {//断开重连

  if(preTCPConnected == true){

    preTCPConnected = false;
    preTCPStartTick = millis();
    Serial.println();
    Serial.println("TCP Client disconnected.");
    TCPclient.stopAll();
  }
  else if(millis() - preTCPStartTick > 1*1000)//重新连接
    TCPclient.stopAll();
    startTCPClient();
  }
  else
  {
    if (TCPclient.available()) {//收数据
      char c =TCPclient.read();
      TcpClient_Buff +=c;
      TcpClient_BuffIndex++;
      TcpClient_preTick = millis();
      
      if(TcpClient_BuffIndex>=MAX_PACKETSIZE - 1){
        TcpClient_BuffIndex = MAX_PACKETSIZE-2;
        TcpClient_preTick = TcpClient_preTick - 200;
      }
      preHeartTick = millis();
    }
    if(millis() - preHeartTick >= KEEPALIVEATIME){//保持心跳
      preHeartTick = millis();
      Serial.println("--Keep alive:");
      sendtoTCPServer("cmd=0&msg=keep\r\n");
    }
  }
  if((TcpClient_Buff.length() >= 1) && (millis() - TcpClient_preTick>=200))
  {//data ready
    TCPclient.flush();
    Serial.println("Recieve: ");
    Serial.println(TcpClient_Buff);
    parseQueryString(TcpClient_Buff);
   TcpClient_Buff="";
   TcpClient_BuffIndex = 0;
  }
}

void startSTA(){
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(DEFAULT_STASSID, DEFAULT_STAPSW);

}



/**************************************************************************
                                 WIFI
***************************************************************************/
/*
  WiFiTick
  检查是否需要初始化WiFi
  检查WiFi是否连接上，若连接成功启动TCP Client
  控制指示灯
*/
void doWiFiTick(){
  static bool startSTAFlag = false;
  static bool taskStarted = false;
  static uint32_t lastWiFiCheckTick = 0;

  if (!startSTAFlag) {
    startSTAFlag = true;
    startSTA();
    Serial.printf("Heap size:%d\r\n", ESP.getFreeHeap());
  }

  //未连接1s重连
  if ( WiFi.status() != WL_CONNECTED ) {
    if (millis() - lastWiFiCheckTick > 1000) {
      lastWiFiCheckTick = millis();
    }
    taskStarted = false;
  }
  //连接成功建立
  else {
    if (taskStarted == false) {
      taskStarted = true;
      Serial.print("\r\nGet IP Address: ");
      Serial.println(WiFi.localIP());
      startTCPClient();
    }
  }
}

// 处理LED灯操作
void handleLedOn() {
  digitalWrite(ledPin, LOW); // NodeMCU的LED是低电平触发，用来调试状态
  digitalWrite(button, HIGH);
  Serial.println("LED ON");
  pressStatus = true;
  pressTime = millis();
  // server.send(200, "text/plain", "LED已打开");
}

void handleLedOff() {
  digitalWrite(ledPin, HIGH);
  digitalWrite(button, LOW);
  Serial.println("LED OFF");
  pressStatus = false;
  // server.send(200, "text/plain", "LED已关闭");
}

void CheckButtonTick()
{
  unsigned long currenttime = millis();
  if(pressStatus == true)
  {
    if((currenttime - pressTime) >= (maxtime)){
      handleLedOff();
    }
  }
}

// 初始化，相当于main 函数
void setup() {
  Serial.begin(115200);
  pinMode(ledPin,OUTPUT);
  pinMode(pwr,OUTPUT);
  pinMode(rst,OUTPUT);
  digitalWrite(ledPin, HIGH);
  // digitalWrite(12, LOW);
  // digitalWrite(13, LOW);
}

//循环
void loop() {
  doWiFiTick();
  doTCPClientTick();
  CheckButtonTick();
}
