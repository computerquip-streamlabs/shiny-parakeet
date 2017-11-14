#include <cstdio>

#include <asio.hpp>
#include <Windows.h>

namespace ipc {

asio::io_service g_service;

namespace client {

class session {
public:
	session(asio::io_service &service, HANDLE pipe) 
	: stream(service, pipe), service(service), work(service)
	{
	}

	void handle_write(const asio::error_code &e, std::size_t bt)
	{
		static int counter = 0;
		printf("Write Operation: %s\n", e.message().c_str());

		Sleep(1000);

		if (counter < 10) {
			start();
		} else {
			stream.close();
			service.stop();
		}

		counter++;
	}

	void start()
	{
		const char test[] = "!@#%$^SDF;oiji";

		asio::async_write(
			stream,
			asio::const_buffer((void*)test, strlen(test)),
			[this](const asio::error_code &e, std::size_t bt) {
				this->handle_write(e, bt);
			}
		);
	}

	asio::io_service &service;
	asio::io_service::work work;
	uint8_t read_buffer[4096];
	uint8_t write_buffer[4096];
	asio::windows::stream_handle stream;
};

}

}

const auto PIPE_NAME = "\\\\.\\pipe\\bobby";

void handle_connect(const asio::error_code &ec, std::size_t bytes_transferred)
{
	printf("Connected~!");
}

int main(int argc, char *argv[]) 
{
	WaitNamedPipe(PIPE_NAME, NMPWAIT_WAIT_FOREVER);

	printf("Connecting...\n");
	HANDLE hClientPipe = 
	CreateFile(
		PIPE_NAME,
		GENERIC_READ | GENERIC_WRITE,
		0,
		0,
		OPEN_EXISTING,
		FILE_FLAG_OVERLAPPED,
		0
	);

	if (hClientPipe == INVALID_HANDLE_VALUE) {
		printf("Failed to connect to pipe!\n");
		return 1;
	}

	ipc::client::session session(ipc::g_service, hClientPipe);
	session.start();
	ipc::g_service.run();
}