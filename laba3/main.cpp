#include "my_shmem.hpp"
#include <iostream>
#include <string>
#include <fstream>
#include <chrono>
#include <thread>
#include <Windows.h>
#include <unistd.h>
#include <mutex>

struct global_variables
{
	// std::string path = "..//log.txt";
	int counter = 0;
	int processes[4] = {0, 0, 0, 0}; // 0 - изначальный, 1 - копия 1, 2 - копия 2, 3 - открытые пользователем
};

void write_log(std::string s){
	std::ofstream file;
	file.open("..//log.txt", std::fstream::app);
	if (file){
		file << s;
		// std::cout << s << std::endl;
	} else {
		std::cout << "Invalid path!" << std::endl;
	}
	file.close();
}

void write_wait(int time_ms, global_variables* shared_data, std::mutex* shr_mutex, 		std::mutex* file_lock){
	while (true){
		std::this_thread::sleep_for(std::chrono::milliseconds(time_ms));
		std::string s;
		shr_mutex->lock();
		s = std::to_string(shared_data->counter) + '\n';
		shr_mutex->unlock();
		file_lock->lock();
		write_log(s);
		file_lock->unlock();
	}
}

void count_wait(int time_ms, global_variables* shared_data, std::mutex* shr_mutex){
	while (true){
		std::this_thread::sleep_for(std::chrono::milliseconds(time_ms));
		shr_mutex->lock();
		shared_data->counter++;
		shr_mutex->unlock();
	}
}

void spawn_copy(int time_ms, global_variables* shared_data, std::mutex* shr_mutex){
	while (true){
		std::this_thread::sleep_for(std::chrono::milliseconds(time_ms));
		shr_mutex->lock();
		int processes[] = {shared_data->processes[0], shared_data->processes[1], shared_data->processes[2], shared_data->processes[3]};
		if (processes[1] != 0){
			std::cout << "Process type 1 not finished." << std::endl;
		} else {
			STARTUPINFO si = {sizeof(si)};
			PROCESS_INFORMATION pi;
			LPSTR args = "1";
			bool bCP = CreateProcess("./laba3.exe", args, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
			// WaitForSingleObject(pi.hProcess, INFINITE);
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
		}
		if (processes[2] != 0){
			std::cout << "Process type 2 not finished." << std::endl;
		} else {
			STARTUPINFO si = {sizeof(si)};
			PROCESS_INFORMATION pi;
			LPSTR args = "2";
			bool bCP = CreateProcess("./laba3.exe", args, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
			// WaitForSingleObject(pi.hProcess, INFINITE);
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
		}
		shr_mutex->unlock();
	}
}

int main(int argc, char** argv)
{
	HANDLE file_mapping = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(global_variables), "my_shared_memory");
    global_variables* shared_data = static_cast<global_variables*>(MapViewOfFile(file_mapping, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, sizeof(global_variables)));

	std::mutex shr_mutex;
	std::mutex file_lock;
	int process_role;
	
	shr_mutex.lock();
	if (shared_data->processes[0] == 0){
		process_role = 0;
	} else if (atoi(argv[0]) == 1){
		process_role = 1;
	} else if (atoi(argv[0]) == 2){
		process_role = 2;
	} else {
		process_role = 3;
	}
	shr_mutex.unlock();

	if (process_role == 0){
		shr_mutex.lock();
		shared_data->processes[0]++;
		shr_mutex.unlock();
		file_lock.lock();
		write_log("Pid: " + std::to_string(getpid()) + "\t" + "dummy time" + '\n');
		file_lock.unlock();
		std::thread cnt_w0(count_wait, 300, shared_data, &shr_mutex);
		std::thread wr_w0(write_wait, 1000, shared_data, &shr_mutex, &file_lock);
		std::thread sp_c0(spawn_copy, 3000, shared_data, &shr_mutex);
		std::string input;
		int cnt;
		while (true){
			std::cin >> input;
			if (input == "cnt"){
				std::cout << "Input counter value:" << std::endl;
				std::cin >> cnt;
				shr_mutex.lock();
				shared_data->counter = cnt;
				shr_mutex.unlock();
			} else {
				std::cout << "Unknown input, write 'cnt' to input counter value." << std::endl;
			}
		}
	}
	
	else if (process_role == 1){
		shr_mutex.lock();
		shared_data->processes[1]++;
		shr_mutex.unlock();

		write_log("Pid: " + std::to_string(getpid()) + "\t" + "dummy time" + '\n');
		shr_mutex.lock();
		shared_data->counter++;
		shr_mutex.unlock();
		write_log("Pid: " + std::to_string(getpid()) + "\t" + "exiting at " + "\t" + "dummy time" + '\n');
		
		shr_mutex.lock();
		shared_data->processes[1]--;
		shr_mutex.unlock();
	} 
	
	else if (process_role == 2){
		shr_mutex.lock();
		shared_data->processes[2]++;
		shr_mutex.unlock();

		write_log("Pid: " + std::to_string(getpid()) + "\t" + "dummy time" + '\n');
		shr_mutex.lock();
		shared_data->counter *= 2;
		shr_mutex.unlock();
		std::this_thread::sleep_for(std::chrono::milliseconds(2000));
		shr_mutex.lock();
		shared_data->counter /= 2;
		shr_mutex.unlock();
		write_log("Pid: " + std::to_string(getpid()) + "\t" + "exiting at " + "\t" + "dummy time" + '\n');
		
		shr_mutex.lock();
		shared_data->processes[2]--;
		shr_mutex.unlock();
	}
	
	else if (process_role == 3){
		std::thread cnt_w0(count_wait, 300, shared_data, &shr_mutex);
		std::string input;
		int cnt;
		while (true){
			std::cin >> input;
			if (input == "cnt"){
				std::cout << "Input counter value:" << std::endl;
				std::cin >> cnt;
				shr_mutex.lock();
				shared_data->counter = cnt;
				shr_mutex.unlock();
			} else {
				std::cout << "Unknown input, write 'cnt' to input counter value." << std::endl;
			}
		}
	}

    return 0;
}