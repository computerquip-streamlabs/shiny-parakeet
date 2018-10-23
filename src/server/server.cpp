#include <cstdio>

#include <asio.hpp>
#include <Windows.h>

namespace ipc {

/*
	Note that when we call async_read,
	the function will not return until a
	condition has completed. By default,
	it's whenever the buffer is full.

	This means that if our client only gives
	10 bytes of data at a time, it will not
	return until that happens 100 times.
 */

/******************************************************************************
 * The individual server. Represents each connection to the server.
 *****************************************************************************/
template <size_t BufferSize = 4096>
class server {
	struct session {
		session(asio::io_context &io_ctx, HANDLE handle)
		: stream(io_ctx, handle)
		{}

		asio::windows::stream_handle stream;

		uint8_t read_buffer[BufferSize] { 0 };
		uint8_t write_buffer[BufferSize] { 0 };
	};

	const char *name;
	asio::io_context io_ctx;
	std::list<session> sessions;

	void handle_connection(
	  session &session,
	  const asio::error_code &e,
	  std::size_t bt
	);

	void handle_async_read(
	  session &session,
	  asio::error_code error,
	  std::size_t length
	);

	void listen(session &session);

public:
	explicit server(const char *name);
	~server();

	/* Each connection is the same so no parameters
	 * are allowed here. */

	void add_session();
	void listen();

	/* Remove session won't be removed until
	 * it's finished its current job. */
	void remove_session();

	/* Signal each session to close */
	void close();

	/* Execute handlers on this thread */
	void run();
};

template <size_t BufferSize>
server<BufferSize>::server(const char *name)
: name(name)
{
}

template <size_t BufferSize>
server<BufferSize>::~server()
{
}

template <size_t BufferSize>
void server<BufferSize>::run()
{
	io_ctx.run();
}

template <size_t BufferSize>
void server<BufferSize>::add_session()
{
	const DWORD dwOpenMode =
		PIPE_ACCESS_DUPLEX |
		FILE_FLAG_OVERLAPPED;

	const DWORD dwPipeMode =
		PIPE_TYPE_BYTE |
		PIPE_READMODE_BYTE |
		PIPE_WAIT;

	/* A reasonable guess */
	const DWORD dwTimeout = 0;

	/* Not used */
	LPSECURITY_ATTRIBUTES lpSecurityAttributes = NULL;

	HANDLE hServerPipe = CreateNamedPipe(
		name,
		dwOpenMode,
		dwPipeMode,
		PIPE_UNLIMITED_INSTANCES,
		BufferSize /* Out Buffer */,
		BufferSize /* In Buffer */,
		dwTimeout,
		lpSecurityAttributes
	);

	sessions.emplace_back(io_ctx, hServerPipe);
}

template <size_t BufferSize>
void server<BufferSize>::handle_async_read(
  server::session &session,
  asio::error_code error,
  std::size_t length
) {
	if (error) {
		printf("Failed to read from pipe: %s\n",
		       error.message().c_str());

		if (error.value() != ERROR_BROKEN_PIPE) return;

		printf("Client closed the handle.\n");
		BOOL okay = DisconnectNamedPipe(session.stream.native_handle());

		if (!okay) {
			printf("Failed to reset pipe!\n");
			io_ctx.stop();
		}

		session.stream.close();

		return;
	}

	printf("Message Read: %i: %.*s\n", length, length, session.read_buffer);

	session.stream.async_read_some(
		asio::mutable_buffer(session.read_buffer, BufferSize),
		[this, &session](asio::error_code error, std::size_t length) {
			this->handle_async_read(session, error, length);
		}
	);
}

template <size_t BufferSize>
void server<BufferSize>::handle_connection(
  server::session &session,
  const asio::error_code &e,
  std::size_t bt
) {
	printf("Overlapped Event: %s\n", e.message().c_str());

	add_session();
	listen(sessions.back());

	session.stream.async_read_some(
		asio::mutable_buffer(session.read_buffer, BufferSize),
		[this, &session](asio::error_code error, std::size_t length) {
			handle_async_read(session, error, length);
		}
	);
}

template <size_t BufferSize>
void server<BufferSize>::listen(server::session &session)
{
	asio::windows::overlapped_ptr overlapped(
		io_ctx,
		[this, &session](const asio::error_code &e, std::size_t bt) {
			handle_connection(session, e, bt);
		}
	);

	BOOL connected = ConnectNamedPipe(
		session.stream.native_handle(), overlapped.get());

	DWORD connect_error = GetLastError();

	if (!connected && connect_error != ERROR_IO_PENDING) {
		asio::error_code ec(
			connect_error,
			asio::error::get_system_category()
		);
		overlapped.complete(ec, 0);
	} else {
		printf("Pending...\n");
		overlapped.release();
	}
}

template <size_t BufferSize>
void server<BufferSize>::listen()
{
	for (auto &session : sessions) {
		listen(session);
	}
}

} // namespace ipc

/*
 * I would like to think this code as a guide:
  * https://svn.boost.org/svn/boost/sandbox/process/boost/process/detail/pipe.hpp
 */

int main(int argc, char *argv[])
{
	/* There's a big difference between the number of sessions
	 * and the number of threads. You can have one thread but
	 * a ton of sessions. However, it's somewhat pointless to
	 * have multiple threads but only one session. It doesn't
	 * scale well. There's a golden area here as a result.*/

	/* Number of simultaneous listeners. Notice the actual number
	 * of sessions will dynamically increase */
	const char *pipe_name = "\\\\.\\pipe\\bobby";
	const int num_sessions = 1;
	const int num_threads = 2;
	std::vector<std::thread> threads;

	ipc::server<4096> server(pipe_name);

	for (int i = 0; i < num_sessions; ++i)
		server.add_session();

	/* Give the server work so it doesn't immediately quit */
	server.listen();

	for (int i = 0; i < num_threads; ++i) {
		threads.emplace_back([&server] () {
			server.run();
		});
	}

	for (auto &thread : threads) {
		thread.join();
	}
}