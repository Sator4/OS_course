#include "my_serial.hpp"
#include <sstream>              // std::stringstream
#include <iostream>             // std::cout
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <algorithm>

#if !defined (_WIN32)
#	include <unistd.h>          // pause()
#	include <time.h>            // nanosleep()
#endif
// Сконвертировать любой базовый тип в строку
template<class T> std::string to_string(const T& v)
{
    std::ostringstream ss;
    ss << v;
    return ss.str();
}

struct average_temp{
    average_temp(float sum, int time_from, int entries_count){
        this->sum = sum;
        this->time_from = time_from;
        this->entries_count = entries_count;
    }
    float sum;
    int time_from;
    int entries_count;
};

void csleep(double timeout) {
#if defined (_WIN32)
	if (timeout <= 0.0)
        ::Sleep(INFINITE);
    else
        ::Sleep((DWORD)(timeout * 1e3));
#else
    if (timeout <= 0.0)
        pause();
    else {
        struct timespec t;
        t.tv_sec = (int)timeout;
        t.tv_nsec = (int)((timeout - t.tv_sec)*1e9);
        nanosleep(&t, NULL);
    }
#endif
}

void write_log(std::string s, std::string path){
	std::ofstream file;
	file.open(path, std::fstream::app);
	if (file){
		file << s;
	} else {
		std::cout << "Invalid path!" << std::endl;
	}
	file.close();
}


std::string to_time(int seconds){
    time_t time = seconds;
    struct tm tstruct;
    char buf[80];
    tstruct = *localtime(&time);
    strftime(buf, sizeof(buf), "%d-%m-%Y %X", &tstruct);
    return to_string(buf);
}

void purge(std::string path, int timestamp){
    std::ifstream file;
    std::ofstream temp;
    std::string s;
    std::string temp_path = path.substr(0, path.length() - 4) + ".temp.txt";
    file.open(path);
    temp.open(temp_path);
    while (std::getline(file, s)){
        int current_time = std::stoi(s.substr(s.length()-10, s.length()-1));
        std::cout << current_time << std::endl;
        if (current_time > timestamp){
            temp << s << std::endl;
        }
    }
    temp.close();
    file.close();
    const char * p = path.c_str();
    remove(p);
    rename(temp_path.c_str(), p);
}


int main(int argc, char** argv)
{
	if (argc < 3) {
		std::cout << "Usage: sertest [port] [regime], where [regime] can be 'read' or 'write'" << std::endl;
		return -1;
	}
	bool read = true;
	if (!strcmp(argv[2],"write"))
		read = false;
	
	cplib::SerialPort smport(std::string(argv[1]),cplib::SerialPort::BAUDRATE_115200);
    
	if (!smport.IsOpen()) {
		std::cout << "Failed to open port '" << argv[1] << "'! Terminating..." << std::endl;
		return -2;
	}

    int polling_rate_virtual = 300;   // seconds
    float polling_rate_real_time = 1.0;


	if (read) {
        std::string path_all = "..//log_all.txt";
        std::string path_hour = "..//log_hour.txt";
        std::string path_day = "..//log_day.txt";
        std::string mystr;
        average_temp hour_average(0, time(0)/3600 * 3600, 0);
        average_temp day_average(0, time(0)/86400 * 86400, 0);
		smport.SetTimeout(polling_rate_real_time);
		for (;;) {
			smport >> mystr;
            if (mystr.length() == 0){
                continue;
            }
            float current_temperature;
            int current_time;
            int stage = 1;
            int slice = 0;
            for (int i = 0; i < mystr.length(); i++){
                if (mystr[i] == ' '){
                    if (stage == 1){
                        current_temperature = stof(mystr.substr(slice, i));
                    }
                    if (stage == 3){
                        slice = i+1;
                    }
                    if (stage == 4){
                        current_time = std::stoi(mystr.substr(slice, i));
                    }
                    stage++;
                }
            }
            if (current_time < 1000000000){
                continue;
            }
            write_log(to_string(current_temperature) + ' ' + to_time(current_time) + ' ' + to_string(current_time) + '\n', path_all);
            hour_average.sum += current_temperature;
            hour_average.entries_count++;
            day_average.sum += current_temperature;
            day_average.entries_count++;
            purge(path_all, current_time - 3600);

            
            if (current_time - hour_average.time_from > 3600){
                write_log(to_string(hour_average.sum / hour_average.entries_count) + ' ' + to_time(current_time) + ' ' + to_string(current_time) + '\n', path_hour);
                hour_average.time_from = current_time / 3600 * 3600;
                hour_average.entries_count = 0;
                hour_average.sum = 0;
                purge(path_hour, current_time - 86400);
            }
            if (current_time - day_average.time_from > 86400){
                write_log(to_string(day_average.sum / day_average.entries_count) + ' ' + to_time(current_time) + ' ' + to_string(current_time) + '\n', path_day);
                day_average.time_from = current_time / 86400 * 86400;
                day_average.entries_count = 0;
                day_average.sum = 0;
                purge(path_day, current_time - 86400 * 365.25);
            }
		}
	} else {
        std::string mystr;
        int current_time = time(0);
        std::srand(std::time(nullptr));
        float current_temperature = 20;
		for (int i = 0;;i++) {
            current_temperature += float(std::rand() - RAND_MAX / 2) / float(RAND_MAX) / float(10);
            mystr = to_string(current_temperature) + ' ' + to_time(current_time) + ' ' + to_string(current_time) + ' ';
            smport << mystr;
            current_time += polling_rate_virtual;
			csleep(polling_rate_real_time);
		}
	}
    return 0;
}