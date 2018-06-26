/*  Serial port object for use with wxWidgets
 *  Copyright 2010, Paul Stoffregen (paul@pjrc.com)
 *  Modified by: Leonardo Laguna Ruiz
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "serial.h"

#include <windows.h>
#include <fcntl.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#define win32_err(s) FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, \
			GetLastError(), 0, (s), sizeof(s), NULL)
  #define QUERYDOSDEVICE_BUFFER_SIZE 262144
  #if _MSC_VER
	#define snprintf _snprintf_s
  #endif


Serial::Serial()
{
	port_is_open = 0;
	baud_rate = 38400;	// default baud rate
}

Serial::~Serial()
{
	Close();
}



// Open a port, by name.  Return 0 on success, non-zero for error
int Serial::Open(const string& name)
{
	Close();

	COMMCONFIG cfg;
	COMMTIMEOUTS timeouts;
	int got_default_cfg=0, port_num;
	char buf[1024], name_createfile[64], name_commconfig[64], *p;
	DWORD len;

	snprintf(buf, sizeof(buf), "%s", name.c_str());
	p = strstr(buf, "COM");
	if (p && sscanf(p + 3, "%d", &port_num) == 1) {
		//printf("port_num = %d\n", port_num);
		snprintf(name_createfile, sizeof(name_createfile), "\\\\.\\COM%d", port_num);
		snprintf(name_commconfig, sizeof(name_commconfig), "COM%d", port_num);
	} else {
		snprintf(name_createfile, sizeof(name_createfile), "%s", name.c_str());
		snprintf(name_commconfig, sizeof(name_commconfig), "%s", name.c_str());
	}
	len = sizeof(COMMCONFIG);
	if (GetDefaultCommConfig(name_commconfig, &cfg, &len)) {
		// this prevents unintentionally raising DTR when opening
		// might only work on COM1 to COM9
		got_default_cfg = 1;
		memcpy(&port_cfg_orig, &cfg, sizeof(COMMCONFIG));
		cfg.dcb.fDtrControl = DTR_CONTROL_DISABLE;
		cfg.dcb.fRtsControl = RTS_CONTROL_DISABLE;
		SetDefaultCommConfig(name_commconfig, &cfg, sizeof(COMMCONFIG));
	} else {
		printf("error with GetDefaultCommConfig\n");
	}
	port_handle = CreateFile(name_createfile, GENERIC_READ | GENERIC_WRITE,
	   0, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
	if (port_handle == INVALID_HANDLE_VALUE) {
		win32_err(buf);
		error_msg =  "Unable to open " + name + ", " + buf;
		return -1;
	}
	len = sizeof(COMMCONFIG);
	if (!GetCommConfig(port_handle, &port_cfg, &len)) {
		CloseHandle(port_handle);
		win32_err(buf);
		error_msg = "Unable to read communication config on " + name + ", " + buf;
		return -1;
	}
	if (!got_default_cfg) {
		memcpy(&port_cfg_orig, &port_cfg, sizeof(COMMCONFIG));
	}
	// http://msdn2.microsoft.com/en-us/library/aa363188(VS.85).aspx
	port_cfg.dcb.BaudRate = baud_rate;
	port_cfg.dcb.fBinary = TRUE;
	port_cfg.dcb.fParity = FALSE;
	port_cfg.dcb.fOutxCtsFlow = FALSE;
	port_cfg.dcb.fOutxDsrFlow = FALSE;
	port_cfg.dcb.fDtrControl = DTR_CONTROL_ENABLE;
	port_cfg.dcb.fDsrSensitivity = FALSE;
	port_cfg.dcb.fTXContinueOnXoff = TRUE;	// ???
	port_cfg.dcb.fOutX = FALSE;
	port_cfg.dcb.fInX = FALSE;
	port_cfg.dcb.fErrorChar = FALSE;
	port_cfg.dcb.fNull = FALSE;
	port_cfg.dcb.fRtsControl = RTS_CONTROL_DISABLE;
	port_cfg.dcb.fAbortOnError = FALSE;
	port_cfg.dcb.ByteSize = 8;
	port_cfg.dcb.Parity = NOPARITY;
	port_cfg.dcb.StopBits = ONESTOPBIT;
	if (!SetCommConfig(port_handle, &port_cfg, sizeof(COMMCONFIG))) {
		CloseHandle(port_handle);
		win32_err(buf);
		error_msg = "Unable to write communication config to " + name + ", " + buf;
		return -1;
	}
	if (!EscapeCommFunction(port_handle, CLRDTR | CLRRTS)) {
		CloseHandle(port_handle);
		win32_err(buf);
		error_msg = "Unable to control serial port signals on " + name + ", " + buf;
		return -1;
	}
	// http://msdn2.microsoft.com/en-us/library/aa363190(VS.85).aspx
	// setting to all zeros means timeouts are not used
	//timeouts.ReadIntervalTimeout		= 0;
	timeouts.ReadIntervalTimeout		= MAXDWORD;
	timeouts.ReadTotalTimeoutMultiplier	= 0;
	timeouts.ReadTotalTimeoutConstant	= 0;
	timeouts.WriteTotalTimeoutMultiplier	= 0;
	timeouts.WriteTotalTimeoutConstant	= 0;
	if (!SetCommTimeouts(port_handle, &timeouts)) {
		CloseHandle(port_handle);
		win32_err(buf);
		error_msg = "Unable to write timeout settings to " + name + ", " + buf;
		return -1;
	}
	port_name = name;
	port_is_open = 1;
	return 0;
}

string Serial::get_name(void)
{
	if (!port_is_open) return "";
	return port_name;
}



// Return the last error message in a (hopefully) user friendly format
string Serial::error_message(void)
{
	return error_msg;
}

int Serial::Is_open(void)
{
	return port_is_open;
}

// Close the port
void Serial::Close(void)
{
	if (!port_is_open) return;
	Output_flush();
	Input_discard();
	port_is_open = 0;
	port_name = "";
	//SetCommConfig(port_handle, &port_cfg_orig, sizeof(COMMCONFIG));
	CloseHandle(port_handle);
}

// set the baud rate
int Serial::Set_baud(int baud)
{
	if (baud <= 0) return -1;
	//printf("set_baud: %d\n", baud);
	baud_rate = baud;
	if (!port_is_open) return -1;
	DWORD len = sizeof(COMMCONFIG);
	port_cfg.dcb.BaudRate = baud;
	SetCommConfig(port_handle, &port_cfg, len);
	return 0;
}

int Serial::Set_baud(const string& baud_str)
{
	unsigned long b;
	b = atoi(baud_str.c_str());
	if (!b) return -1;
	return Set_baud((int)b);
}


// Read from the serial port.  Returns only the bytes that are
// already received, up to count.  This always returns without delay,
// returning 0 if nothing has been received
int Serial::Read(void *ptr, int count)
{
	if (!port_is_open) return -1;
	if (count <= 0) return 0;
	// first, we'll find out how many bytes have been received
	// and are currently waiting for us in the receive buffer.
	//   http://msdn.microsoft.com/en-us/library/ms885167.aspx
	//   http://msdn.microsoft.com/en-us/library/ms885173.aspx
	//   http://source.winehq.org/WineAPI/ClearCommError.html
	COMSTAT st;
	DWORD errmask=0, num_read, num_request;
	OVERLAPPED ov;
	int r;
	if (!ClearCommError(port_handle, &errmask, &st)) return -1;
	//printf("Read, %d requested, %lu buffered\n", count, st.cbInQue);
	if (st.cbInQue <= 0) return 0;
	// now do a ReadFile, now that we know how much we can read
	// a blocking (non-overlapped) read would be simple, but win32
	// is all-or-nothing on async I/O and we must have it enabled
	// because it's the only way to get a timeout for WaitCommEvent
	num_request = ((DWORD)count < st.cbInQue) ? (DWORD)count : st.cbInQue;
	ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (ov.hEvent == NULL) return -1;
	ov.Internal = ov.InternalHigh = 0;
	ov.Offset = ov.OffsetHigh = 0;
	if (ReadFile(port_handle, ptr, num_request, &num_read, &ov)) {
		// this should usually be the result, since we asked for
		// data we knew was already buffered
		//printf("Read, immediate complete, num_read=%lu\n", num_read);
		r = num_read;
	} else {
		if (GetLastError() == ERROR_IO_PENDING) {
			if (GetOverlappedResult(port_handle, &ov, &num_read, TRUE)) {
				//printf("Read, delayed, num_read=%lu\n", num_read);
				r = num_read;
			} else {
				//printf("Read, delayed error\n");
				r = -1;
			}
		} else {
			//printf("Read, error\n");
			r = -1;
		}
	}
	CloseHandle(ov.hEvent);
	// TODO: how much overhead does creating new event objects on
	// every I/O request really cost?  Would it be better to create
	// just 3 when we open the port, and reset/reuse them every time?
	// Would mutexes or critical sections be needed to protect them?
	return r;
}

// Write to the serial port.  This blocks until the data is
// sent (or in a buffer to be sent).  All bytes are written,
// unless there is some sort of error.
int Serial::Write(const void *ptr, int len)
{
	//printf("Write %d\n", len);
	if (!port_is_open) return -1;
	DWORD num_written;
	OVERLAPPED ov;
	int r;
	ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (ov.hEvent == NULL) return -1;
	ov.Internal = ov.InternalHigh = 0;
	ov.Offset = ov.OffsetHigh = 0;
	if (WriteFile(port_handle, ptr, len, &num_written, &ov)) {
		//printf("Write, immediate complete, num_written=%lu\n", num_written);
		r = num_written;
	} else {
		if (GetLastError() == ERROR_IO_PENDING) {
			if (GetOverlappedResult(port_handle, &ov, &num_written, TRUE)) {
				//printf("Write, delayed, num_written=%lu\n", num_written);
				r = num_written;
			} else {
				//printf("Write, delayed error\n");
				r = -1;
			}
		} else {
			//printf("Write, error\n");
			r = -1;
		}
	};
	CloseHandle(ov.hEvent);
	return r;
}

// Wait up to msec for data to become available for reading.
// return 0 if timeout, or non-zero if one or more bytes are
// received and can be read.  -1 if an error occurs
int Serial::Input_wait(int msec)
{
	if (!port_is_open) return -1;
	// http://msdn2.microsoft.com/en-us/library/aa363479(VS.85).aspx
	// http://msdn2.microsoft.com/en-us/library/aa363424(VS.85).aspx
	// http://source.winehq.org/WineAPI/WaitCommEvent.html
	COMSTAT st;
	DWORD errmask=0, eventmask=EV_RXCHAR, ret;
	OVERLAPPED ov;
	int r;
	// first, request comm event when characters arrive
	if (!SetCommMask(port_handle, EV_RXCHAR)) return -1;
	// look if there are characters in the buffer already
	if (!ClearCommError(port_handle, &errmask, &st)) return -1;
	//printf("Input_wait, %lu buffered, timeout = %d ms\n", st.cbInQue, msec);
	if (st.cbInQue > 0) return 1;

	ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (ov.hEvent == NULL) return -1;
	ov.Internal = ov.InternalHigh = 0;
	ov.Offset = ov.OffsetHigh = 0;
	if (WaitCommEvent(port_handle, &eventmask, &ov)) {
		//printf("Input_wait, WaitCommEvent, immediate success\n");
		r = 1;
	} else {
		if (GetLastError() == ERROR_IO_PENDING) {
			ret = WaitForSingleObject(ov.hEvent, msec);
			if (ret == WAIT_OBJECT_0) {
				//printf("Input_wait, WaitCommEvent, delayed success\n");
				r = 1;
			} else if (ret == WAIT_TIMEOUT) {
				//printf("Input_wait, WaitCommEvent, timeout\n");
				GetCommMask(port_handle, &eventmask);
				r = 0;
			} else {  // WAIT_FAILED or WAIT_ABANDONED
				//printf("Input_wait, WaitCommEvent, delayed error\n");
				r = -1;
			}
		} else {
			//printf("Input_wait, WaitCommEvent, immediate error\n");
			r = -1;
		}
	}
	SetCommMask(port_handle, 0);
	CloseHandle(ov.hEvent);
	return r;
}

// Discard all received data that hasn't been read
void Serial::Input_discard(void)
{
	if (!port_is_open) return;
	PurgeComm(port_handle, PURGE_RXCLEAR);
}

// Wait for all transmitted data with Write to actually leave the serial port
void Serial::Output_flush(void)
{
	if (!port_is_open) return;
	FlushFileBuffers(port_handle);
}


// set DTR and RTS,  0 = low, 1=high, -1=unchanged
int Serial::Set_control(int dtr, int rts)
{
	if (!port_is_open) return -1;
	// http://msdn.microsoft.com/en-us/library/aa363254(VS.85).aspx
	if (dtr == 1) {
		if (!EscapeCommFunction(port_handle, SETDTR)) return -1;
	} else if (dtr == 0) {
		if (!EscapeCommFunction(port_handle, CLRDTR)) return -1;
	}
	if (rts == 1) {
		if (!EscapeCommFunction(port_handle, SETRTS)) return -1;
	} else if (rts == 0) {
		if (!EscapeCommFunction(port_handle, CLRRTS)) return -1;
	}
	return 0;
}

// Return a list of all serial ports
vector<string> Serial::port_list()
{
	vector<string> list;
	// http://msdn.microsoft.com/en-us/library/aa365461(VS.85).aspx
	// page with 7 ways - not all of them work!
	// http://www.naughter.com/enumser.html
	// may be possible to just query the windows registary
	// http://it.gps678.com/2/ca9c8631868fdd65.html
	// search in HKEY_LOCAL_MACHINE\HARDWARE\DEVICEMAP\SERIALCOMM
	// Vista has some special new way, vista-only
	// http://msdn2.microsoft.com/en-us/library/aa814070(VS.85).aspx
	char *buffer, *p;
	//DWORD size = QUERYDOSDEVICE_BUFFER_SIZE;
	DWORD ret;

	buffer = (char *)malloc(QUERYDOSDEVICE_BUFFER_SIZE);
	if (buffer == NULL) return list;
	memset(buffer, 0, QUERYDOSDEVICE_BUFFER_SIZE);
	ret = QueryDosDeviceA(NULL, buffer, QUERYDOSDEVICE_BUFFER_SIZE);
	if (ret) {
		//printf("Detect Serial using QueryDosDeviceA: ");
		for (p = buffer; *p; p += strlen(p) + 1) {
			if (strncmp(p, "COM", 3)) continue;
			list.push_back(string(p) );
			//printf(":  %s\n", p);
		}
	} else {
		char buf[1024];
		win32_err(buf);
		printf("QueryDosDeviceA failed, error \"%s\"\n", buf);
		printf("Detect Serial using brute force GetDefaultCommConfig probing: ");
		for (int i=1; i<=32; i++) {
			printf("try  %s", buf);
			COMMCONFIG cfg;
			DWORD len;
			snprintf(buf, sizeof(buf), "COM%d", i);
			if (GetDefaultCommConfig(buf, &cfg, &len)) {
				std::ostringstream name;
  				name << "COM" << i << ":";
				list.push_back(name.str());
				printf(":  %s", buf);
			}
		}
	}
	free(buffer);
	std::sort (list.begin(), list.end());
	return list;
}