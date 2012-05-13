#include "ctb-0.16/ctb.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <iostream>
#include <sstream>
#include <string>
#include "httppil.h"
#include "httpapi.h"

using namespace std;
vector<ctb::SerialPort*> serials;
ctb::SerialPort* serialPort = 0;

extern "C" int uhSerial(UrlHandlerParam* param)
{

	char* port = mwGetVarValue(param->pxVars, "port", "");
	vector<ctb::SerialPort*>::iterator it;
	int size = serials.size();
	cerr << "[SERIAL] " << size << " opened ports";
	for (it = serials.begin() ; it < serials.end(); it++) {
		char *s = (*it)->m_devname;
		if (!strncmp(s, "\\\\.\\", 4)) {
			s += 4;
		}
		if (!strcmp(s, port)) {
			serialPort = *it;
			break;
		}
	}

    if (!*param->pucRequest) {
        vector<string> ports;
        stringstream s;
		s << "<?xml version=\"1.0\"?>"
			<< "<SerialPorts>";
        if( ctb::GetAvailablePorts( ports )) {
            for( int i = 0; i < ports.size(); i++) {
				s << "<Port>" << ports[i] << "</Port>";
            }
        } else {
            s << "<Error/>";
        }
		s << "</SerialPorts>";
		param->dataBytes = _snprintf(param->pucBuffer, param->dataBytes, "%s", s.str().c_str());
		param->fileType=HTTPFILETYPE_XML;
    } else if (!strcmp(param->pucRequest, "/close")) {
        if (serialPort) {
			for (it = serials.begin() ; it < serials.end(); it++) {
				if (*it == serialPort) {
					serials.erase(it);
					delete serialPort;
					serialPort = 0;
					break;
				}
			}
            param->dataBytes = sprintf(param->pucBuffer, "Port closed");
        } else {
            param->dataBytes = sprintf(param->pucBuffer, "No port opened");
        }
		param->fileType=HTTPFILETYPE_TEXT;
	} else if (!strcmp(param->pucRequest, "/read")) {
		int timeout = mwGetVarValueInt(param->pxVars, "timeout", 0);
		char* eos = mwGetVarValue(param->pxVars, "eos", 0);
		if (eos) {
			if (serialPort->ReadUntilEOS(param->pucBuffer, (size_t*)&param->dataBytes, eos, timeout) == -1)
				param->dataBytes = 0;
		} else {
			param->dataBytes = serialPort->Read(param->pucBuffer, param->dataBytes);
		}
		if (param->dataBytes < 0) {
			param->dataBytes = 0;
		}
		param->fileType=HTTPFILETYPE_TEXT;
	} else if (!strcmp(param->pucRequest, "/write") && 
		param->hs->request.payloadSize > 0 &&
		param->hs->request.payloadSize == param->hs->dataLength) {
		if (serialPort) {
			int timeout = mwGetVarValueInt(param->pxVars, "timeout", 1000);
			int delay = mwGetVarValueInt(param->pxVars, "delay", 0);
			int echo = mwGetVarValueInt(param->pxVars, "echo", 0);
			int retries = mwGetVarValueInt(param->pxVars, "retries", 0);
			int payloadSize = param->hs->request.payloadSize;
			int written = 0;
			if (delay == 0 && echo == 0) {
				written = serialPort->Write(param->pucPayload, payloadSize);
			} else if (echo) {
				if (delay == 0) delay = 10;
				do {
					written = 0;
					for (int i = 0; i < payloadSize; i++, written++) {
						if (serialPort->Write(param->pucPayload + i, 1) != 1) {
							break;
						}
						char c = 0;
						DWORD startTime = GetTickCount();
						do {
							if (serialPort->Read(&c, 1) != -1 && c == param->pucPayload[i])
								break;
							Sleep(delay);
						} while (GetTickCount() - startTime < timeout);
						if (c != param->pucPayload[i])
							break;
					}
				} while (written < payloadSize && retries-- > 0);
			} else {
				for (int i = 0; i < payloadSize; i++, written++) {
					if (serialPort->Write(param->pucPayload + i, 1) != 1) {
						break;
					}
					Sleep(delay);
				}
			}
			param->dataBytes = sprintf(param->pucBuffer, "%d bytes written", written);
		} else {
			param->dataBytes = sprintf(param->pucBuffer, "No port opened");
		}
		param->fileType=HTTPFILETYPE_TEXT;
    } else if (!strcmp(param->pucRequest, "/open")) {
        int baudrate = mwGetVarValueInt(param->pxVars, "baudrate", 9600);
        char* proto = mwGetVarValue(param->pxVars, "protocol", "8N1");
		char sport[16];
		_snprintf(sport, sizeof(sport), isdigit(port[3]) && atoi(port + 3) >= 10 ? "\\\\.\\%s" : "%s", port);
		if (!serialPort) {
			bool success = false;
            serialPort = new ctb::SerialPort();
			if(success = (serialPort->Open(sport, baudrate, proto, ctb::SerialPort::NoFlowControl ) >= 0)) {
				param->dataBytes = sprintf(param->pucBuffer, "OK - %s opened", port);
			} else {
				param->dataBytes = sprintf(param->pucBuffer, "Error opening %s", port);
			}
			if (!success) {
				delete serialPort;
				serialPort = 0;
			} else {
				serials.push_back(serialPort);
			}
		} else {
			param->dataBytes = sprintf(param->pucBuffer, "%s already opened", port);
		}
		param->fileType=HTTPFILETYPE_TEXT;
    } else if (!strcmp(param->pucRequest, "/setline")) {
		int state = mwGetVarValueInt(param->pxVars, "DTR", -1);
		if (state == 1) {
			serialPort->SetLineState(ctb::LinestateDtr);
		} else if (state == 0) {
			serialPort->ClrLineState(ctb::LinestateDtr);
		}
		state = mwGetVarValueInt(param->pxVars, "RTS", -1);
		if (state == 1) {
			serialPort->SetLineState(ctb::LinestateRts);
		} else if (state == 0) {
			serialPort->ClrLineState(ctb::LinestateRts);
		}
		state = mwGetVarValueInt(param->pxVars, "CTS", -1);
		if (state == 1) {
			serialPort->SetLineState(ctb::LinestateCts);
		} else if (state == 0) {
			serialPort->ClrLineState(ctb::LinestateCts);
		}
		param->dataBytes = sprintf(param->pucBuffer, "OK");
    } else {
        return 0;
    }
    cerr << "[SERIAL] " << param->pucBuffer << endl;
    return FLAG_DATA_RAW;
}
