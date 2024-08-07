/****************************************************************************************************************************
  FTPClient_Generic_Impl.h

  FTP Client for Generic boards using SD, FS, etc.

  Based on and modified from

  1) esp32_ftpclient Library         https://github.com/ldab/ESP32_FTPClient

  Built by Khoi Hoang https://github.com/khoih-prog/FTPClient_Generic

  Version: 1.6.0

  Version Modified By   Date      Comments
  ------- -----------  ---------- -----------
  1.0.0   K Hoang      11/05/2022 Initial porting and coding to support many more boards, using WiFi or Ethernet
  1.1.0   K Hoang      13/05/2022 Add support to Teensy 4.1 using QNEthernet or NativeEthernet
  1.2.0   K Hoang      14/05/2022 Add support to other FTP Servers. Fix bug
  1.2.1   K Hoang      14/05/2022 Auto detect server response type in PASV mode
  1.3.0   K Hoang      16/05/2022 Fix uploading issue of large files for WiFi, QNEthernet
  1.4.0   K Hoang      05/11/2022 Add support to ESP32/ESP8266 using Ethernet W5x00 or ENC28J60
  1.5.0   K Hoang      20/01/2023 Add support to RP2040W using `arduino-pico` core
  1.5.0   K Hoang      20/01/2023 Add support to Ethernet W6100 using Ethernet_Generic library
 *****************************************************************************************************************************/

#pragma once

#ifndef FTPCLIENT_GENERIC_IMPL_H
#define FTPCLIENT_GENERIC_IMPL_H

#include "FTPClient_Generic.hpp"
#include "ftpparse.h"

#if !defined(USING_NEW_PASSIVE_MODE_ANSWER_TYPE)
  #define USING_NEW_PASSIVE_MODE_ANSWER_TYPE    true
#endif

/////////////////////////////////////////////

FTPClient_Generic::FTPClient_Generic(const char* _serverAdress, uint16_t _port, const char* _userName, const char* _passWord,
                                     uint16_t _timeout)
{
  userName      = strdup(_userName);
  passWord      = strdup(_passWord);
  serverAdress  = strdup(_serverAdress);
  port          = _port;
  timeout       = _timeout;
}

/////////////////////////////////////////////

FTPClient_Generic::FTPClient_Generic(const char* _serverAdress, const char* _userName, const char* _passWord, uint16_t _timeout)
{
  userName      = strdup(_userName);
  passWord      = strdup(_passWord);
  serverAdress  = strdup(_serverAdress);
  port          = FTP_PORT;
  timeout       = _timeout;
}

FTPClient_Generic::~FTPClient_Generic() {
  if (userName) {
    free(userName);
  }
  if (passWord) {
    free(passWord);
  }
  if (serverAdress) {
    free(serverAdress);
  }
}

/////////////////////////////////////////////

theFTPClient* FTPClient_Generic::GetDataClient()
{
  return dclient;
}

/////////////////////////////////////////////

bool FTPClient_Generic::isConnected()
{
  if (!_isConnected)
  {
    FTP_LOGWARN1("FTP error: ", outBuf);
  }

  return _isConnected;
}

/////////////////////////////////////////////

void FTPClient_Generic::GetLastModifiedTime(const char  * fileName, char* result, size_t len)
{
  FTP_LOGINFO("Send MDTM");

  if (!isConnected())
  {
    FTP_LOGERROR("GetLastModifiedTime: Not connected error");
    return;
  }

  client->print(COMMAND_FILE_LAST_MOD_TIME);
  client->println(fileName);
  GetFTPAnswer(result, len);
}

/////////////////////////////////////////////

void FTPClient_Generic::WriteClientBuffered(theFTPClient* cli, unsigned char * data, int dataLength)
{
  if (!isConnected())
    return;

  size_t clientCount = 0;

  for (int i = 0; i < dataLength; i++)
  {
    clientBuf[clientCount] = data[i];
    clientCount++;

    if (clientCount > bufferSize - 1)
    {
#if FTP_CLIENT_USING_QNETHERNET
      cli->writeFully(clientBuf, bufferSize);
#else
      cli->write(clientBuf, bufferSize);
#endif

      FTP_LOGDEBUG3("Written: num bytes = ", bufferSize, ", index = ", i);
      FTP_LOGDEBUG3("Written: clientBuf = ", (uint32_t) clientBuf, ", clientCount = ", clientCount);

      clientCount = 0;
    }
  }

  if (clientCount > 0)
  {
    cli->write(clientBuf, clientCount);

    FTP_LOGDEBUG1("Last Written: num bytes = ", clientCount);
  }
}

/////////////////////////////////////////////

int FTPClient_Generic::GetFTPAnswer(char* result, size_t len)
{
  int ret = 0;
  char * endPtr = NULL;
  char thisByte;
  outCount = 0;

  unsigned long _m = millis();

  while (!client->available() && millis() < _m + timeout)
    delay(100);

  if ( !client->available())
  {
    memset( outBuf, 0, sizeof(outBuf) );
    strcpy( outBuf, "Offline");

    _isConnected = false;
    isConnected();

    return ret;
  }

  while (client->available())
  {
    thisByte = client->read();

    if (outCount < sizeof(outBuf))
    {
      if ((outCount == 0) && ((thisByte == ' ') || (thisByte == '\r') || (thisByte == '\n'))) {
        //ignore initial spaces
        continue;
      } else if (thisByte == '\n') {
        //process only one answer at a time
        break;
      }
      outBuf[outCount] = thisByte;
      outCount++;
      outBuf[outCount] = 0;
    }
  }

  if (outBuf[0] == '4' || outBuf[0] == '5' )
  {
    _isConnected = false;
    isConnected();
  } 
  else
  {
    _isConnected = true;
  }

  ret = strtol(outBuf, &endPtr, 10);

  if (result) {
    //strip off result code (3 digits) and spaces
    if (endPtr) {
      int start = endPtr - outBuf;
      for (size_t i = start; i < sizeof(outBuf); i++) {
        if (endPtr[0] == ' ') {
          endPtr++;
        } else {
          break;
        }
      }
      start = endPtr - outBuf;
      memcpy(result, endPtr, min(sizeof(outBuf) - start, len));
    } else {
      memset(result, 0, len);
    }
    FTP_LOGDEBUG2("Result: ", ret, result);
  } else {
    FTP_LOGDEBUG2("Result: ", ret, endPtr);
  }

  return ret;
}


int FTPClient_Generic::TryGetFTPAnswer(char* result, size_t len)
{
  int ret = -1;
  char * endPtr = NULL;
  char thisByte;
  outCount = 0;


  while (client->available())
  {
    thisByte = client->read();

    if (outCount < sizeof(outBuf))
    {
      if ((outCount == 0) && ((thisByte == ' ') || (thisByte == '\r') || (thisByte == '\n'))) {
        //ignore initial spaces
        continue;
      } else if (thisByte == '\n') {
        //process only one answer at a time
        break;
      }
      outBuf[outCount] = thisByte;
      outCount++;
      outBuf[outCount] = 0;
    }
  }

  if (outCount > 0) {

    if (outBuf[0] == '4' || outBuf[0] == '5' )
    {
      _isConnected = false;
      isConnected();
    } 
    else
    {
      _isConnected = true;
    }

    ret = strtol(outBuf, &endPtr, 10);

    if (result) {
      //strip off result code (3 digits) and spaces
      if (endPtr) {
        int start = endPtr - outBuf;
        for (size_t i = start; i < sizeof(outBuf); i++) {
          if (endPtr[0] == ' ') {
            endPtr++;
          } else {
            break;
          }
        }
        start = endPtr - outBuf;
        memcpy(result, endPtr, min(sizeof(outBuf) - start, len));
      } else {
        memset(result, 0, len);
      }
      FTP_LOGDEBUG2("Result: ", ret, result);
    } else {
      FTP_LOGDEBUG2("Result: ", ret, endPtr);
    }
  }
  return ret;
}


/////////////////////////////////////////////

void FTPClient_Generic::WriteData (unsigned char * data, int dataLength)
{
  FTP_LOGDEBUG(F("Writing"));

  if (!isConnected())
  {
    FTP_LOGERROR("WriteData: Not connected error");
    return;
  }

  FTP_LOGDEBUG1("WriteData: datalen = ", dataLength);

  WriteClientBuffered(dclient, &data[0], dataLength);
}

/////////////////////////////////////////////

void FTPClient_Generic::CloseFile ()
{
  FTP_LOGDEBUG(F("Close File"));
  dclient->stop();

  if (!isConnected())
  {
    FTP_LOGERROR("CloseFile: Not connected error");
    return;
  }

  GetFTPAnswer();
}

bool FTPClient_Generic::WaitCloseOrTransferComplete() {
  bool ret = false;
  bool bClosed = false;
  bool bAnswer = false;
  FTP_LOGDEBUG(F("WaitCloseOrTransferComplete"));

  unsigned long _m = millis();

  while (millis() < _m + timeout) {    
    if (!dclient->connected()) {
      if (!bClosed) {
        FTP_LOGDEBUG2("Data socket closed by server after ", (millis() - _m), " ms");
        bClosed = true;
      }
      ret = true;
    }

    int res = TryGetFTPAnswer();
    if (res > 0) {
      if (!bAnswer) {
        FTP_LOGDEBUG2("Ftp answer after ", (millis() - _m), " ms");
        bAnswer = true;
      }
    }
    if (res == TRANSFER_COMPLETE) {
      ret = true;
    }

    if (bClosed && bAnswer) {
      break;
    }
    delay(100);
  }

  if (!isConnected())
  {
    FTP_LOGERROR("WaitCloseOrTransferComplete: Not connected error");
  }

  return ret;
}


/////////////////////////////////////////////

void FTPClient_Generic::Write(const char * str)
{
  FTP_LOGDEBUG(F("Write File"));

  if (!isConnected())
  {
    FTP_LOGERROR("Write: Not connected error");
    return;
  }

  GetDataClient()->print(str);
}

/////////////////////////////////////////////

void FTPClient_Generic::CloseConnection()
{
  client->println(COMMAND_QUIT);
  client->stop();
  FTP_LOGINFO(F("Connection closed"));
}

/////////////////////////////////////////////

void FTPClient_Generic::OpenConnection(theFTPClient  * cmdClient, theFTPClient  * dataClient)
{
  FTP_LOGINFO1(F("Connecting to: "), serverAdress);

  client = cmdClient;
  dclient = dataClient;

  if ( client->connect(serverAdress, port) )
  {
    FTP_LOGINFO(F("Command connected"));
  }

  GetFTPAnswer();

  FTP_LOGINFO1("Send USER = ", userName);

  client->print(COMMAND_USER);
  client->println(userName);

  GetFTPAnswer();

  FTP_LOGINFO1("Send PASSWORD = ", passWord);

  client->print(COMMAND_PASS);
  client->println(passWord);

  GetFTPAnswer();
}

/////////////////////////////////////////////

void FTPClient_Generic::RenameFile(const char* from, const char* to)
{
  FTP_LOGINFO("Send RNFR");

  if (!isConnected())
  {
    FTP_LOGERROR("RenameFile: Not connected error");
    return;
  }

  client->print(COMMAND_RENAME_FILE_FROM);
  client->println(from);

  GetFTPAnswer();

  FTP_LOGINFO("Send RNTO");

  client->print(COMMAND_RENAME_FILE_TO);
  client->println(to);

  GetFTPAnswer();
}

/////////////////////////////////////////////

void FTPClient_Generic::NewFile (const char* fileName)
{
  FTP_LOGINFO("Send STOR");

  if (!isConnected())
  {
    FTP_LOGERROR("NewFile: Not connected error");
    return;
  }

  client->print(COMMAND_FILE_UPLOAD);
  client->println(fileName);

  GetFTPAnswer();
}

/////////////////////////////////////////////

bool FTPClient_Generic::InitFile(const char* type)
{
  bool res = false;
  int ret = 0;
  FTP_LOGINFO1("Send TYPE", type);

  if (!isConnected())
  {
    FTP_LOGERROR("InitFile: Not connected error");
    return res;
  }

  FTP_LOGINFO("Send PASV");

  client->println(COMMAND_PASSIVE_MODE);
  ret = GetFTPAnswer();
  if ((ret < 100) || (ret >= 400)) {
    return res;
  }
  // KH
  FTP_LOGDEBUG1("outBuf =", outBuf);

  char *tmpPtr;
  //FTP_LOGDEBUG1("outBuf =", strtol(outBuf, &tmpPtr, 10 ));

  while (strtol(outBuf, &tmpPtr, 10 ) != ENTERING_PASSIVE_MODE)
  {
    client->println(COMMAND_PASSIVE_MODE);
    ret = GetFTPAnswer();
    if ((ret < 100) || (ret >= 400)) {
      return res;
    }
    FTP_LOGDEBUG1("outBuf =", outBuf);
    delay(1000);
  }

  // Test to know which format
  // 227 Entering Passive Mode (192,168,2,112,157,218)
  // 227 Entering Passive Mode (4043483328, port 55600)
  char *passiveIP = strchr(outBuf, '(') + 1;
  //FTP_LOGDEBUG1("passiveIP =", atoi(passiveIP));

  if (atoi(passiveIP) <= 0xFF)
  {
    char *tStr = strtok(outBuf, "(,");
    int array_pasv[6];

    for ( int i = 0; i < 6; i++)
    {
      tStr = strtok(NULL, "(,");

      //FTP_LOGDEBUG1("tStr =", tStr);

      if (tStr == NULL)
      {
        FTP_LOGDEBUG(F("Bad PASV Answer"));

        CloseConnection();
        return res;
      }

      array_pasv[i] = atoi(tStr);
    }

    unsigned int hiPort, loPort;
    hiPort = array_pasv[4] << 8;
    loPort = array_pasv[5] & 255;

    _dataAddress = IPAddress(array_pasv[0], array_pasv[1], array_pasv[2], array_pasv[3]);

    _dataPort = hiPort | loPort;

    FTP_LOGDEBUG1(F("Data port: "), _dataPort);
  }
  else
  {
    // Using with old style PASV answer, such as `FTP_Server_Teensy41` library

    //char *subStr = strchr(outBuf, '(') + 1;
    char *ptr = strtok(passiveIP, ",");
    uint32_t ret = strtoul( ptr, &tmpPtr, 10 );

    // get IP of data client
    _dataAddress = IPAddress(ret);

    passiveIP = strchr(outBuf, ')');
    ptr = strtok(passiveIP, "port ");

    _dataPort = strtol( ptr, &tmpPtr, 10 );
  }

  FTP_LOGINFO3(F("_dataAddress: "), _dataAddress, F(", Data port: "), _dataPort);

  if (dclient->connect(_dataAddress, _dataPort))
  {
    FTP_LOGDEBUG(F("Data connection established"));
    res = true;
  }

  client->println(type);
  ret = GetFTPAnswer();
  if ((ret < 100) || (ret >= 400)) {
    return false;
  }

  return res;
}

/////////////////////////////////////////////

void FTPClient_Generic::AppendFile (const char* fileName)
{
  FTP_LOGINFO("Send APPE");

  if (!isConnected())
  {
    FTP_LOGERROR("AppendFile: Not connected error");
    return;
  }

  client->print(COMMAND_APPEND_FILE);
  client->println(fileName);
  GetFTPAnswer();
}

/////////////////////////////////////////////

bool FTPClient_Generic::ChangeWorkDir(const char * dir)
{
  FTP_LOGINFO("Send CWD");

  if (!isConnected())
  {
    FTP_LOGERROR("ChangeWorkDir: Not connected error");
    return false;
  }

  client->print(COMMAND_CURRENT_WORKING_DIR);
  client->println(dir);
  int ret = GetFTPAnswer();
  if ((ret < 100) || (ret >= 400)) {
    return false;
  }
  return true;
}

/////////////////////////////////////////////

void FTPClient_Generic::DeleteFile(const char * file)
{
  FTP_LOGINFO("Send DELE");

  if (!isConnected())
  {
    FTP_LOGERROR("DeleteFile: Not connected error");
    return;
  }

  client->print(COMMAND_DELETE_FILE);
  client->println(file);
  GetFTPAnswer();
}

/////////////////////////////////////////////

void FTPClient_Generic::MakeDir(const char * dir)
{
  FTP_LOGINFO("Send MKD");

  if (!isConnected())
  {
    FTP_LOGERROR("MakeDir: Not connected error");
    return;
  }

  client->print(COMMAND_MAKE_DIR);
  client->println(dir);

  GetFTPAnswer();
}

/////////////////////////////////////////////

void FTPClient_Generic::RemoveDir(const char * dir)
{
  FTP_LOGINFO("Send RMD");

  if (!isConnected())
  {
    FTP_LOGERROR("RemoveDir: Not connected error");
    return;
  }

  client->print(COMMAND_REMOVE_DIR);
  client->println(dir);

  GetFTPAnswer();
}

/////////////////////////////////////////////

size_t FTPClient_Generic::ContentList(const char * dir, FTPListEntry * list, size_t sz)
{
  char _resp[ sizeof(outBuf) ];
  size_t _b = 0xFFFFFFFF;  //error
  int ret = 0;

  FTP_LOGINFO("Send MLSD");

  if (!isConnected())
  {
    FTP_LOGERROR("ContentList: Not connected error");
    return _b;
  }

  client->print(COMMAND_LIST_DIR_STANDARD);
  client->println(dir);

  ret = GetFTPAnswer(_resp, sizeof(outBuf));
  if ((ret < 100) || (ret >= 400)) {
    return _b;
  }


  // Convert char array to string to manipulate and find response size
  // each server reports it differently, TODO = FEAT
  //String resp_string = _resp;
  //resp_string.substring(resp_string.lastIndexOf('matches')-9);
  //FTP_LOGDEBUG(resp_string);

  unsigned long _m = millis();

  while ( !dclient->available() && millis() < _m + timeout)
    delay(1);

  _b = 0;

  while (dclient->available())
  {
    if ( _b < sz )
    {
      //TODO: parse facts and return only if Type=file or Type=dir
      list[_b].name = dclient->readStringUntil('\n');
      //FTP_LOGDEBUG(String(_b) + ":" + list[_b]);
      _b++;
    }
  }
  return _b;
}

/////////////////////////////////////////////

size_t FTPClient_Generic::ContentListWithListCommand(const char * dir, FTPListEntry * list, size_t sz)
{
  char _resp[ sizeof(outBuf) ];
  size_t _b = 0xFFFFFFFF;  //error
  int ret = 0;

  FTP_LOGINFO("Send LIST");

  if (!isConnected())
  {
    FTP_LOGERROR("ContentListWithListCommand: Not connected error");
    return _b;
  }

  client->print(COMMAND_LIST_DIR);
  client->println(dir);

  ret = GetFTPAnswer(_resp, sizeof(outBuf));
  if ((ret < 100) || (ret >= 400)) {
    return _b;
  }

  // Convert char array to string to manipulate and find response size
  // each server reports it differently, TODO = FEAT
  //String resp_string = _resp;
  //resp_string.substring(resp_string.lastIndexOf('matches')-9);
  //FTP_LOGDEBUG(resp_string);

  unsigned long _m = millis();

  while ( !dclient->available() && millis() < _m + timeout)
    delay(1);

  _b = 0;

  while (dclient->available())
  {
    if ( _b < sz )
    {
      String tmp = dclient->readStringUntil('\n');
      if (tmp[tmp.length() - 1] == '\r') {
        tmp = tmp.substring(0, tmp.length() - 1);
      }
      FTP_LOGDEBUG(String(_b) + ":" + tmp);
      struct ftpparse item;
      int parse = ftpparse(&item, (char*) tmp.c_str(), tmp.length());
      if (parse) {
        list[_b].name = item.name;
        list[_b].isDirectory = (item.flagtrycwd != 0);
        list[_b].size = ((item.flagtrycwd == 0) ? item.size : 0);
        FTP_LOGDEBUG3(_b, list[_b].name, (list[_b].isDirectory ? "D" : ""), list[_b].size);
      } else {
        if ((tmp[0] == 'd') || (tmp[0] == 'D')) {
          list[_b].isDirectory = true;
        } else {
          list[_b].isDirectory = false;
        }
        list[_b].name = tmp.substring(tmp.lastIndexOf(" ") + 1, tmp.length());
      }
      
      _b++;
    }
  }
  return _b;
}

/////////////////////////////////////////////

void FTPClient_Generic::DownloadString(const char * filename, String &str)
{
  FTP_LOGINFO("Send RETR");

  if (!isConnected())
    return;

  client->print(COMMAND_DOWNLOAD);
  client->println(filename);

  char _resp[ sizeof(outBuf) ];
  GetFTPAnswer(_resp, sizeof(outBuf));

  unsigned long _m = millis();

  while ( !GetDataClient()->available() && millis() < _m + timeout)
    delay(1);

  while ( GetDataClient()->available() )
  {
    str += GetDataClient()->readString();
  }
}

/////////////////////////////////////////////

uint32_t FTPClient_Generic::DownloadFile(const char * filename, unsigned char * buf, size_t length, bool printUART )
{
  uint32_t res = 0;
  FTP_LOGINFO("Send RETR");

  if (!isConnected())
  {
    FTP_LOGERROR("DownloadFile: Not connected error");
    return res;
  }

  client->print(COMMAND_DOWNLOAD);
  client->println(filename);

  char _resp[ sizeof(outBuf) ];
  GetFTPAnswer(_resp, sizeof(outBuf));

  char _buf[2];

  unsigned long _m = millis();

  while ( !dclient->available() && millis() < _m + timeout)
    delay(1);

  while (dclient->available())
  {
    if ( !printUART )
      res += dclient->readBytes(buf, length);
    else
    {
      for (size_t _b = 0; _b < length; _b++ )
      {
        dclient->readBytes(_buf, 1);
        res++;
        //FTP_LOGDEBUG0(_buf[0]);
      }
    }
  }
  return res;
}

/////////////////////////////////////////////

uint32_t FTPClient_Generic::DownloadProgressive(const char * filename, uint8_t * buffer, size_t bufferLen, FTPDownloadCallback downloadCallback, void * userData) 
{
  uint32_t res = 0;
  bool keepDownloading = true;

  FTP_LOGINFO("Send RETR");

  if (!isConnected())
  {
    FTP_LOGERROR("DownloadFile: Not connected error");
    return res;
  }

  client->print(COMMAND_DOWNLOAD);
  client->println(filename);

  GetFTPAnswer();

  unsigned long _m = millis();

  while ( !dclient->available() && millis() < _m + timeout)
    delay(1);

  while (keepDownloading) {
    while (dclient->available())
    {
      size_t sz = dclient->readBytes(buffer, bufferLen);
      downloadCallback(filename, buffer, sz, userData);
      _m = millis();
      res += sz;
    }
    if ((!dclient->connected()) || (millis() >= _m + timeout)) {
      FTP_LOGDEBUG("Download end");
      keepDownloading = false;
    }
    delay(10);
  }
  return res;
}

/////////////////////////////////////////////

uint32_t FTPClient_Generic::GetFileSize(const char * filename)
{
  FTP_LOGINFO("Send SIZE");

  if (!isConnected())
    return 0xFFFFFFFF;

  client->print(COMMAND_SIZE);
  client->println(filename);

  char _resp[ sizeof(outBuf) ];
  GetFTPAnswer(_resp, sizeof(outBuf));
  
  uint32_t ret = (uint32_t) atoi(_resp);
  return ret;
}

/////////////////////////////////////////////

#endif    // FTPCLIENT_GENERIC_IMPL_H
