/*
 Name:		iHelper_Receiver.ino
 Created:	2/3/2016 2:52:22 PM
 Author:	Pranav
*/

#define USE_SERIAL Serial
#define DEBUG
#define DEBUG_HTTPCLIENT Serial.printf

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <Wire.h>

//EEPROM STRUCTURE
/*
IDend length of Hkey =hardware key loaded at boot
Ukey of length Ukeylen
Last configured ssid and pwd may or maynot be required
OFFSET including the above
Relaystatus of length NUMRELAY loaded at boot
Relaymodes of length NUMRELAY loaded at boot
schedules
*/

#define DS3231_I2C_ADDRESS 0x68
#define NUMHOPS 5 //no of hops fromt the main router to the leaf 
#define URL "www.mysh.net23.net"
#define PORT 80
#define NUMRELAY 2
#define RELAYstart 12// gpio pin no from which relay is connected
#define MY_PWD "passphrase"
#define MY_PREFIX "iHelp"
#define Hkeypart 4
#define SERVER_IP_ADDR "192.168.4.1"
#define SERVER_PORT 2123
#define Server_WaitResponse "@wait "
#define ERROR1 "@wait E1@"
#define OFFSET 101
byte RELAYstatus[NUMRELAY] = { 'O', 'F' };
byte RELAYmodes[NUMRELAY] = { 'M', 'M' };
String ssid, password, Ukey;
byte requestCounter = 0;
byte totalQuery = 0;
enum ConnectionMode{ INTERNET, NO_INTERNET, TO_ROUTER, TO_MESH, NONE };

// this is the hardware key that is linked with the hardware
// and must be changed everytime on flashing the hardware and shall be associated to ONE USER ONLY
String Hkey;
//byte IDend = Hkey.length() + 1;
#define IDend 16
#define Ukeylen 21
#define SSIDlen 32
#define PASSWORDlen 20


//HTTPClient http;

ESP8266WebServer server(2123);// to initialise a server that connects at port 80
String exceptions;
ConnectionMode connection = NONE;
struct query {
	char HID[17];
	char ques[30];
	char response[30];
	query *next;
	query *prev;
};
query *temp, *currentQuery;
void addQuery(const char hid[], const char answer[], const char ques[]){
	Serial.printf("add q total %d", totalQuery);
	if (currentQuery == NULL)
	{
		currentQuery = new query;
		currentQuery->next = NULL;
		currentQuery->prev = NULL;
		ets_strncpy(currentQuery->HID, hid, sizeof(currentQuery->HID));
		ets_strncpy(currentQuery->response, answer, sizeof(currentQuery->response));
		ets_strncpy(currentQuery->ques, ques, sizeof(currentQuery->ques));
		totalQuery++;

	}
	else
	{
		move2end();//just a precaution
		temp = new query;
		temp->prev = currentQuery;
		temp->next = NULL;
		currentQuery->next = temp;
		ets_strncpy(temp->HID, hid, sizeof(temp->HID));
		ets_strncpy(temp->response, answer, sizeof(temp->response));
		ets_strncpy(temp->ques, ques, sizeof(temp->ques));
		currentQuery = temp;
		totalQuery++;
	}
	Serial.printf("%s,%s,%s\n", currentQuery->HID, currentQuery->ques, currentQuery->response);
}
void removeQuery(){
	query* del = currentQuery;
	if (currentQuery->prev == NULL && currentQuery->next == NULL){ currentQuery = NULL; }
	else if (currentQuery->prev == NULL){
		(currentQuery->next)->prev = currentQuery->prev;
		currentQuery = currentQuery->next;

	}
	else if (currentQuery->next == NULL){
		currentQuery->prev->next = currentQuery->next;
		currentQuery = currentQuery->prev;

	}
	else{
		currentQuery->prev->next = currentQuery->next;
		(currentQuery->next)->prev = currentQuery->prev;
		currentQuery = currentQuery->prev;

	}

	delete del;
	totalQuery--;
	Serial.printf("remv q total %d", totalQuery);
}
int editQuery(const char hid[], const char answer[], const char ques[]){
	if (currentQuery == NULL)
	{
		return -1;
	}
	else
	{
		Serial.println(answer);
		ets_strncpy(currentQuery->HID, hid, sizeof(currentQuery->HID));
		ets_strncpy(currentQuery->response, answer, sizeof(currentQuery->response));
		ets_strncpy(currentQuery->ques, ques, sizeof(currentQuery->ques));
	}
	Serial.printf("editQuery total %d", totalQuery);
}
void move2start(){
	while (currentQuery->prev != NULL){ currentQuery = currentQuery->prev; }
}
void move2end(){
	while (currentQuery->next != NULL){ currentQuery = currentQuery->next; }
}
String isQuery(const char hid[], const char ques[]){
	if (currentQuery == NULL){ return "0"; }
	move2start();
	while (1){
		if (currentQuery->next != NULL){
			if (ets_strcmp(currentQuery->HID, hid) != 0 || ets_strcmp(currentQuery->ques, ques) != 0){
				currentQuery = currentQuery->next;
			}
			else{
				if ((String)currentQuery->response != (String)(Server_WaitResponse + (String)currentQuery->HID + "@")) { return currentQuery->response; }
				move2end(); return "2";
			}
		}
		else {
			if (ets_strcmp(currentQuery->HID, hid) == 0 && ets_strcmp(currentQuery->ques, ques) == 0){
				Serial.print(currentQuery->HID);
				Serial.print(currentQuery->response);
				if ((String)currentQuery->response != (String)(Server_WaitResponse + (String)currentQuery->HID + "@")){ return currentQuery->response; }
				return "2";
			}
			else{ return "0"; }

		}

	}

}
void move2query(byte index){
	move2start();
	index--;
	for (index; index > 0; index--){
		currentQuery = currentQuery->next;
	}
}

bool checkPriority(String sample){
	if (sample.toInt() < Hkey.substring(0, Hkeypart).toInt()){
		return true;
	}
	else{ return false; }
}


String sendGET(const char url[], int port, const char path[], int length){

	String recv;
	String temp;
	temp.reserve(120);
	WiFiClient client;
	delay(1000);
	//	if (client.connect(IPAddress(192,168,4,1), port)){
	if (client.connect(url, port)){
		client.print("GET ");
		client.print(path);
		client.print(" HTTP/1.1\r\n");
		client.print("Host: ");
		client.print(url);
		client.print("\r\nUser-Agent: Mozilla/5.0\r\n");
		client.print("Connection: close\r\n\r\n");
		Serial.println("WOW connected");
	}

	int wait = 1000;
	while (client.connected() && !client.available() && wait--){
		delay(3);
	}
	while (client.available()){
		temp += (char)client.read();
	}
	recv = temp.substring(temp.indexOf("@"), temp.lastIndexOf("@") + 1);
	//// Attempt to make a connection to the remote server
	//if (client.connect("192.168.4.1", 80)) {
	//	Serial.println("\n connect\n");
	//	
	//}
	//else{ Serial.println("not connected\n"); return "not"; }
	//
	//// Make an HTTP GET request
	//client.println("GET /mesh?Hof=htyg&ques=command HTTP/1.1");
	//client.print("Host: ");
	//client.println(url);
	//client.println("Connection: close");
	//client.println();
	//Serial.println("\nmade req\n");
	//
	//
	//	delay(100);
	//	String temp = "";
	//	while (client.available()) {
	//		char c = client.read();
	//			temp += c;
	//		Serial.print(c);
	//	}
	//	
	//	recv = temp.substring(temp.indexOf("@"), temp.lastIndexOf("@") + 1);
	//
	//Serial.printf("%s \n %d \n %s \n", url, port, path);
	/*if (httpCode == 200) {
	String temp= http.getString();
	Serial.println(temp);
	recv=temp.substring(temp.indexOf("@"), temp.lastIndexOf("@")+1);
	}
	else {
	recv = ERROR1;
	}*/
	//Serial.println(recv);
	Serial.flush();
	client.stop();
	return recv;
}
void setStatus(byte relayno, byte status) {
	USE_SERIAL.println("setStatus= ");
	if (relayno <= NUMRELAY) {
		EEPROM.write(OFFSET - 1 + relayno, status);
		RELAYstatus[relayno - 1] = status;
		digitalWrite(RELAYstart + relayno - 1, status);
		USE_SERIAL.printf("%d , %d", relayno, status);
	}
	EEPROM.commit();

}


void configuration()
{
	ssid = server.arg("ssid");
	password = server.arg("pwd");
	String key = server.arg("Hkey");
	if (key == Hkey){
		Ukey = server.arg("Ukey");
		for (byte i = 0; i < Ukeylen; i++){ EEPROM.write(IDend + i, Ukey[i]); }
		WiFi.disconnect();
		WiFi.begin(ssid.c_str(), password.c_str());
		if (WiFi.waitForConnectResult() == WL_CONNECTED){

			if (ssid.indexOf(MY_PREFIX) != -1 && password == MY_PWD){
				connection = TO_MESH;
				server.send(200, "text/html", "connect to mesh node success");
			}
			else{ server.send(200, "text/html", "connect to wifi success"); }
			byte i = 0;
			for (i = 0; i < SSIDlen; i++){ EEPROM.write(IDend + Ukeylen + i, ssid[i]); }
			EEPROM.write(IDend + Ukeylen + i, '\0');
			for (i = 0; i < PASSWORDlen; i++){ EEPROM.write(IDend + Ukeylen + SSIDlen + i, password[i]); }
			EEPROM.write(IDend + Ukeylen + SSIDlen + i, '\0');
		}
		else{
			//couldnt connect to given ssid please change or get in range
			server.send(200, "text/html", "ACK config.not connect");
		}
	}
	USE_SERIAL.print(ssid);
	USE_SERIAL.print("<-ssid  pwd->");
	USE_SERIAL.print(password);
	USE_SERIAL.print("   Ukey->");
	USE_SERIAL.println(Ukey);
	//ukey can be made secure by checking it against the database
	//and the ssid and ip can be stored in the database which receives requests
	//from module.
	//the ukey can be checked against whether an original purchase has been made r not
	EEPROM.commit();
}
/*
void command() {
	String recvCommand = server.arg("command");
	Command cmd(recvCommand);//cost to memory is 112 bytes of ram
	cmd.separate(Hkey);
	server.send(200, "text/html", "ACK babe");
}
*/
void manageMesh(){
	String Hof = server.arg("Hof");
	String question = server.arg("ques");
	Serial.printf("hof=%s ques=%s\n", Hof.c_str(), question.c_str());

	String state = isQuery(Hof.c_str(), question.c_str());
	if (state == "2"){
		//query already there but no response registered
		//send to wait currentQuery point to the end of queue
		server.send(200, "text/plain", Server_WaitResponse + Hof + "@");
	}
	else if (state == "0"){
		//query not found must be added
		//currentQuery points to last query
		String temp = (String)(Server_WaitResponse + Hof + "@");
		addQuery(Hof.c_str(), temp.c_str(), question.c_str());
		server.send(200, "text/plain", Server_WaitResponse + Hof + "@");
	}
	else {
		//query there and response available send back response
		//currentQuery points correctly
		server.send(200, "text/plain", state);
		removeQuery();
	}

}

void connectNode(){
	String key = server.arg("Ukey");
	if (key == Ukey){
		server.send(200, "text/html", Ukey + "@" + exceptions);
		exceptions += server.arg("exceptions");
	}
}
void InitialSetup(){
	//come cback in the end to check for corrections
	//setDS3231time(30, 21, 16, 4, 1, 7, 15);
	String Hkey = "0234567890123456";
	String Ukey = "default";
	byte i = 0;
	for (i = 0; i < IDend; i++) {
		EEPROM.write(i, Hkey[i]);
	}
	for (i = 0; i < Ukeylen - 1; i++) {
		if (Ukey[i] != '\0'){
			EEPROM.write(IDend + i, Ukey[i]);
		}
		else{ EEPROM.write(IDend + i, Ukey[i]); break; }
	}
	for (i = 0; i < NUMRELAY; i++) {
		EEPROM.write(OFFSET + NUMRELAY + i, RELAYmodes[i]);
	}

	EEPROM.commit();

}
bool connectToNode(byte minRSSI){
	int n = WiFi.scanNetworks();

	for (int i = 0; i < n; ++i) {
		String current_ssid = WiFi.SSID(i);
		int index = current_ssid.indexOf(MY_PREFIX);
		
		String target_chip_id = current_ssid.substring(index + sizeof(MY_PREFIX)-1);
		/* Connect to any _suitable_ APs which contain _ssid_prefix */
		if (index != -1 && (target_chip_id != Hkey.substring(0, Hkeypart)) && abs(WiFi.RSSI(i)) < minRSSI ) {
			if (WiFi.status() != WL_CONNECTED){
				WiFi.disconnect();
				WiFi.begin(current_ssid.c_str(), MY_PWD);
				ssid = current_ssid;
				password = MY_PWD;
				delay(2000);
			}

			if (WiFi.waitForConnectResult() != WL_CONNECTED){ continue; }
			else{ return true; }
		}
	}
	if (WiFi.status() != WL_CONNECTED){ return false; }
	else{ return true; }
}
void setup() {

	Hkey.reserve(IDend);
	Ukey.reserve(Ukeylen);
	ssid.reserve(SSIDlen);
	password.reserve(PASSWORDlen);
	exceptions.reserve(5 * NUMHOPS - 1);
	USE_SERIAL.begin(115200);
	EEPROM.begin(512);
	byte i = 0;
	//for (i = 0; i < 100; i++) {
	//	EEPROM.write(i, '\0');
	//}
	//EEPROM.commit();
	//put a button type resetting mechanism
	//InitialSetup();//for new module
	//Read Hkey from EEPROM

	for (i = 0; i < IDend; i++) {
		char a = EEPROM.read(i);
		if (a == '\0'){ break; }
		else { Hkey += a; }
	}
	exceptions = Hkey.substring(0, Hkeypart);
	//UKEY
	for (i = 0; i < Ukeylen - 1; i++) {
		char a = EEPROM.read(IDend + i);
		if (a == '\0'){ break; }
		else{ Ukey += a; }
	}

	//SSID
	for (i = 0; i < SSIDlen; i++) {
		char a = EEPROM.read(Ukeylen + IDend + i);
		if (a == '\0'){ break; }
		else{ ssid += a; }
	}
	//PWD
	for (i = 0; i < PASSWORDlen; i++) {
		char a = EEPROM.read(Ukeylen + IDend + SSIDlen + i);
		if (a == '\0'){ break; }
		else{ password += a; }
	}
	//STATUS
	for (i = 0; i < NUMRELAY; i++) {
		RELAYstatus[i] = EEPROM.read(OFFSET + i);
		digitalWrite(RELAYstart + i, RELAYstatus[i]);
	}
	//MODES
	for (i = 0; i < NUMRELAY; i++) {
		RELAYmodes[i] = EEPROM.read(OFFSET + NUMRELAY + i);
	}
	for (i = 0; i < 100; i++) {
		Serial.printf("%d -> %c \n", i, (char)EEPROM.read(i));
	}

	//USE_SERIAL.setDebugOutput(true);
	WiFi.softAP(((String)MY_PREFIX + Hkey.substring(0, Hkeypart)).c_str(), ((String)MY_PWD).c_str());
	WiFi.mode(WIFI_AP_STA);
	Serial.println(Hkey.c_str());
	Serial.println(Ukey.c_str());
	Serial.println(ssid.c_str());
	Serial.println(password.c_str());
	connectToNode(70);
	//server.on("/command", HTTP_GET, command);
	server.on("/config", HTTP_GET, configuration);
	//server.on("/mesh", HTTP_GET, manageMesh);
	//server.on("/connect", HTTP_GET, connectNode);
	server.begin();
	for (uint8_t t = 5; t > 0 && WiFi.status()!=WL_CONNECTED; t--) {
		USE_SERIAL.printf("SETUP connecting to router %d...\n", t);
		USE_SERIAL.flush();
		delay(1000);
	}
	if (WiFi.waitForConnectResult() == WL_CONNECTED){
		if (WiFi.SSID().indexOf(MY_PREFIX) == 0 && password.compareTo(MY_PWD) == 0){ connection = TO_MESH; }
		else{ connection = TO_ROUTER; }
		Serial.println(WiFi.localIP());
	}

}

/*String getData() {
//here you can send relay statuses to server and any or all sensor data
return (String)counter++;
}
*/
String getResponse(int minRSSI){
	int n = WiFi.scanNetworks();
	String response;
	response.reserve(10);
		for (int i = 0; i < n; ++i) {
			String current_ssid = WiFi.SSID(i);
			int index = current_ssid.indexOf(MY_PREFIX);
			String target_chip_id = current_ssid.substring(index + sizeof(MY_PREFIX));
			/* creat response string with all nearby rssi's and ssid's*/
	if (index != -1 && (target_chip_id != Hkey.substring(0, Hkeypart)) && abs(WiFi.RSSI(i)) < minRSSI ) {
		response += current_ssid;
		response += "=";
		response =response + abs(WiFi.RSSI(i))+",";
			}
		}
		Serial.println(response);
		return response;
}
long double timers = 0;
void loop() {
	Serial.printf("%d <- reqCnt TotQ-> %d \n", requestCounter, totalQuery);
	Serial.printf("%d <- conn wifi->%d", connection, WiFi.status());
	//Serial.printf("%d <- wifi status \n", WiFi.status());
	//please call checkschedule every minute or so

	if (WiFi.status() != WL_CONNECTED){
		connectToNode(65);
	}
	else{
		if (WiFi.SSID().indexOf(MY_PREFIX) == 0 && password.compareTo(MY_PWD) == 0){ connection = TO_MESH; }
		else{ connection = TO_ROUTER; }
	}
	if (WiFi.status() == WL_CONNECTED && connection == TO_MESH){
		WiFi.mode(WIFI_STA);
		delay(100);
		//PROCESS TABLE REQUESTS HERE from mesh basically forward your requests
		
			//run my request
			//String path = SERVER_IP_ADDR;
			String path;
			//			path += ":";
			//			path += SERVER_PORT;
			path += "/mesh?Hof=";
			path += Hkey;
			path += "&state="; 
			path += getResponse(65);//this could be sensor values and other stuff
			String recv = sendGET(SERVER_IP_ADDR, SERVER_PORT, path.c_str(), path.length());
			delay(100);
			WiFi.mode(WIFI_AP_STA);
			
	}
}







