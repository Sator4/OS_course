#include <iostream>
#include <string>
#include <string.h>
#include <fstream>
#include <chrono>
#include <thread>
#include <ctime>
#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <fcntl.h>
#endif
#include <mutex>

struct global_variables
{
	// std::string path = "..//log.txt";
	int counter = 0;
	int processes[4] = {0, 0, 0, 0}; // 0 - изначальный, 1 - копия 1, 2 - копия 2, 3 - открытые пользователем
};

std::string get_now(){
	auto now = std::chrono::high_resolution_clock::now();
	std::time_t timestamp = std::chrono::system_clock::to_time_t(now);
	auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
	struct tm *local_time = std::localtime(&timestamp);
	char buffer[50];
  	std::strftime(buffer, sizeof(buffer), "%H:%M:%S", local_time);
  	std::string formatted_time(buffer);
  	return formatted_time + "." + std::to_string(nanos).substr(10);
}

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

void write_wait(int time_ms, global_variables* shared_data, std::mutex* shr_mutex, std::mutex* file_lock){
	while (true){
		std::this_thread::sleep_for(std::chrono::milliseconds(time_ms));
		std::string s;
		shr_mutex->lock();
		s = "Pid: " + std::to_string(getpid()) + " (parent)" + '\t' + get_now() + "\tcounter = " + std::to_string(shared_data->counter) + '\n';
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

void create_process(const char* role, global_variables* shared_data){
#ifdef _WIN32
	STARTUPINFO si = {sizeof(si)};
	PROCESS_INFORMATION pi;
	LPSTR args = role;
	bool bCP = CreateProcess("./laba3.exe", args, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
	// WaitForSingleObject(pi.hProcess, INFINITE);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
#else
	pid_t pid = fork();
	if (pid == 0){
		execl("./laba3", role, shared_data, (char*)0);
	}
	// std:: cout << pid << std::endl;
#endif
}

void spawn_copy(int time_ms, global_variables* shared_data, std::mutex* shr_mutex, std::mutex* file_lock){
	while (true){
		std::this_thread::sleep_for(std::chrono::milliseconds(time_ms));
		shr_mutex->lock();
		int processes[] = {shared_data->processes[0], shared_data->processes[1], shared_data->processes[2], shared_data->processes[3]};
		if (processes[1] != 0){
			file_lock->lock();
			write_log("Process type 1 not finished.\n");
			file_lock->unlock();
		} else {
			create_process("1", shared_data);
		}
		if (processes[2] != 0){
			file_lock->lock();
			write_log("Process type 2 not finished.\n");
			file_lock->unlock();
		} else {
			create_process("2", shared_data);
		}
		shr_mutex->unlock();
	}
}



int main(int argc, char** argv)
{
#ifdef _WIN32
	HANDLE file_mapping = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(global_variables), "my_shared_memory");
    global_variables* shared_data = static_cast<global_variables*>(MapViewOfFile(file_mapping, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, sizeof(global_variables)));
#else
	const char* shared_nemory_name = "shmtmp";
    int shared_memory_size = sizeof(global_variables);
    int shm_fd = shm_open(shared_nemory_name, O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, shared_memory_size);
    global_variables* shared_data = static_cast<global_variables*>(mmap(0, shared_memory_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0));
	// if (atoi(argv[0]) == 0){
	// 	shared_data->counter = 0;
	// 	shared_data->processes[0] = 0;
	// 	shared_data->processes[1] = 0;
	// 	shared_data->processes[2] = 0;
	// }
#endif
	// std::cout << getpid() << std::endl;
	std::mutex shr_mutex;
	std::mutex file_lock;
	int process_role;
	// std::cout << argc << std::endl;
	
	
	shr_mutex.lock();
	if (atoi(argv[0]) == 0){
		process_role = 0;
		// std::cout << atoi(argv[0]) << std::endl;
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
		write_log("Pid: " + std::to_string(getpid()) + "\t" + get_now() + '\n');
		file_lock.unlock();
		std::thread cnt_w0(count_wait, 300, shared_data, &shr_mutex);
		std::thread wr_w0(write_wait, 1000, shared_data, &shr_mutex, &file_lock);
		std::thread sp_c0(spawn_copy, 3000, shared_data, &shr_mutex, &file_lock);
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
			} else if (input == "q"){
#ifndef _WIN32
				munmap(shared_data, shared_memory_size); shm_unlink("shmtmp");
#endif
				break;
			}
			else {
				std::cout << "Unknown input, write 'cnt' to input counter value or 'q' to quit." << std::endl;
			}
		}
	}
	
	else if (process_role == 1){
		shr_mutex.lock();
		shared_data->processes[1]++;
		shr_mutex.unlock();
		file_lock.lock();
		write_log("Pid: " + std::to_string(getpid()) + " (type 1)" + "\t" + get_now() + '\n');
		file_lock.unlock();
		shr_mutex.lock();
		shared_data->counter++;
		shr_mutex.unlock();
		file_lock.lock();
		write_log("Pid: " + std::to_string(getpid()) + "\t" + "exiting " + get_now() + '\n');
		file_lock.unlock();
		shr_mutex.lock();
		shared_data->processes[1]--;
		shr_mutex.unlock();
	} 
	
	else if (process_role == 2){
		shr_mutex.lock();
		shared_data->processes[2]++;
		shr_mutex.unlock();
		file_lock.lock();
		write_log("Pid: " + std::to_string(getpid()) + " (type 2)" + "\t" + get_now() + '\n');
		file_lock.unlock();
		shr_mutex.lock();
		shared_data->counter *= 2;
		shr_mutex.unlock();
		std::this_thread::sleep_for(std::chrono::milliseconds(2000));
		shr_mutex.lock();
		shared_data->counter /= 2;
		shr_mutex.unlock();
		file_lock.lock();
		write_log("Pid: " + std::to_string(getpid()) + "\t" + "exiting " + get_now() + '\n');
		file_lock.unlock();	
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