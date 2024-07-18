/****************************************************************************************************************************
  FTPClient_Generic_Types.h

 *****************************************************************************************************************************/

#pragma once

#ifndef FTPCLIENT_GENERIC_TYPES_H
#define FTPCLIENT_GENERIC_TYPES_H

typedef struct FTPListEntry {
  String   name;
  bool     isDirectory;
  uint32_t size;
} FTPListEntry;

/////////////////////////////////////////////////////////

#endif  // FTPCLIENT_GENERIC_TYPES_H
