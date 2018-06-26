#ifndef __serial_h__
#define __serial_h__

#include <stdint.h>

#include <windows.h>

#include <vector>
#include <string>
#include <sstream>
#include <algorithm> 

using namespace std;

class Serial
{
public:
	Serial();
	~Serial();
	vector<string> port_list();
	int Open(const string& name);
	string error_message();
	int Set_baud(int baud);
	int Set_baud(const string& baud_str);
	int Read(void *ptr, int count);
	int Write(const void *ptr, int len);
	int Input_wait(int msec);
	void Input_discard(void);
	int Set_control(int dtr, int rts);
	void Output_flush();
	void Close(void);
	int Is_open(void);
	string get_name(void);
private:
	int port_is_open;
	string port_name;
	int baud_rate;
	string error_msg;
private:
	HANDLE port_handle;
	COMMCONFIG port_cfg_orig;
	COMMCONFIG port_cfg;
};

#endif // __serial_h__
