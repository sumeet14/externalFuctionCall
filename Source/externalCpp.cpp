#include<iostream>
#include "external.h"
#include "serial.h"
#include "modelPlugFirmata.h"

using namespace std;

Serial s;

__declspec (dllexport) __stdcall int addNumbers(int a, int b){	
	//return a+b;
	if(s.Is_open())
		s.Close();
	s.Set_baud("115200");
	s.Read((void*)1,1);
	s.Input_wait(100);
	s.Write((void*)1,1);
	s.Input_discard();
	s.Set_control(1,1);
	
	s.Output_flush();
	string ports = s.get_name();
	string err = s.error_message();
	//boardConstructor("COM4", 1, 1, 115200, 1);
	readAnalogPin(9, 0, 1023, 0, 0);
	return s.Open("COM4");

}
//int main(){}