#include <cstdio>

#include <asio.hpp>
#include <Windows.h>

namespace ipc {

asio::io_service g_service;

/*
	Note that when we call async_read, 
	the function will not return until a 
	condition has completed. By default, 
	it's whenever the buffer is full. 

	This means that if our client only gives
	10 bytes of data at a time, it will not 
	return until that happens 100 times. 
 */
const int amount_buffered = 14;

namespace server {

class session {
public:
	session(asio::io_service &service, HANDLE pipe) 
	: stream(service, pipe), work(service), service(service)
	{
	}

	void read() 
	{
		asio::async_read(
			stream,
			asio::mutable_buffer(read_buffer, amount_buffered), 
			[this](asio::error_code error, std::size_t length) {
				if (error) {
					printf("Failed to read from pipe: %s\n", error.message().c_str());

					if (error.value() == ERROR_BROKEN_PIPE) {
						printf("Client closed the handle.\n");
						BOOL okay = DisconnectNamedPipe(stream.native_handle());

						if (!okay) {
							printf("Failed to reset pipe!\n");
							service.stop();
						}

						start();
					}

					return;
				}

				printf("Message Read: %i: %.*s\n", length, length, read_buffer);
				read();
			}
		);
	}

	void handle_connection(const asio::error_code &error, std::size_t bytes_transferred)
	{
		printf("Overlapped Event: %s\n", error.message().c_str());
		
		read();
	}

	void start() 
	{
		asio::windows::overlapped_ptr overlapped(
			service,
			[this](const asio::error_code &e, std::size_t bt) {
				this->handle_connection(e, bt);
			}
		);

		BOOL connected = ConnectNamedPipe(stream.native_handle(), overlapped.get());
		DWORD connect_error = GetLastError();
		
		if (!connected && connect_error != ERROR_IO_PENDING)
		{
			asio::error_code ec(connect_error, asio::error::get_system_category());
			overlapped.complete(ec, 0);
		}
		else
		{
			printf("Pending...\n");
			overlapped.release();
		}
	}

private:
	uint8_t read_buffer[4096];
	uint8_t write_buffer[4096];

	asio::io_service &service;
	asio::io_service::work work;
	asio::windows::stream_handle stream;
};

}

}

/* 
 * I would like to think this code as a guide:
  * https://svn.boost.org/svn/boost/sandbox/process/boost/process/detail/pipe.hpp
 */

int main(int argc, char *argv[])
{
	const DWORD dwOpenMode =
		PIPE_ACCESS_DUPLEX |
		FILE_FLAG_OVERLAPPED;

	const DWORD dwPipeMode = 
		PIPE_TYPE_BYTE |
		PIPE_READMODE_BYTE |
		PIPE_WAIT;

	/* A reasonable guess */
	const DWORD dwPipeSize = 32768;
	const DWORD dwTimeout = 0;

	/* Not used */
	LPSECURITY_ATTRIBUTES lpSecurityAttributes = NULL;

	ipc::server::session *sessions[10];

	for (int i = 0; i < 10; ++i) {
		HANDLE hServerPipe = CreateNamedPipe(
			"\\\\.\\pipe\\bobby",
			dwOpenMode,
			dwPipeMode,
			PIPE_UNLIMITED_INSTANCES,
			dwPipeSize /* Out Buffer */, 
			dwPipeSize /* In Buffer */,
			dwTimeout,
			lpSecurityAttributes
		);

		if (hServerPipe == INVALID_HANDLE_VALUE) {
			printf("Failed to create pipe!");
			return 0;
		}

		sessions[i] = new ipc::server::session(ipc::g_service, hServerPipe);
		sessions[i]->start();
	}

	ipc::g_service.run();

	return 0;
}