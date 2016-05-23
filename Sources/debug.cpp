#include "../V8/include/v8.h"
#include "../V8/include/v8-debug.h"
#include "pch.h"
#include <Kore/Network/Socket.h>
#include <Kore/Threads/Thread.h>

/*
class SendCommandThread;
static SendCommandThread* send_command_thread_ = NULL;

class SendCommandThread : public v8::base::Thread {
public:
	explicit SendCommandThread(v8::Isolate* isolate)
	: Thread(Options("SendCommandThread")),
	semaphore_(0),
	isolate_(isolate) {}
	
	static void CountingAndSignallingMessageHandler(const v8::Debug::Message& message) {
		if (message.IsResponse()) {
			//counting_message_handler_counter++;
			send_command_thread_->semaphore_.Signal();
		}
	}
	
	virtual void Run() {
		semaphore_.Wait();
		const int kBufferSize = 1000;
		uint16_t buffer[kBufferSize];
		const char* scripts_command =
		"{\"seq\":0,"
		"\"type\":\"request\","
		"\"command\":\"scripts\"}";
		int length = AsciiToUtf16(scripts_command, buffer);
		// Send scripts command.
		
		for (int i = 0; i < 20; i++) {
			//v8::base::ElapsedTimer timer;
			//timer.Start();
			//CHECK_EQ(i, counting_message_handler_counter);
			// Queue debug message.
			v8::Debug::SendCommand(isolate_, buffer, length);
			// Wait for the message handler to pick up the response.
			semaphore_.Wait();
			//i::PrintF("iteration %d took %f ms\n", i, timer.Elapsed().InMillisecondsF());
		}
		
		isolate_->TerminateExecution();
	}
	
	void StartSending() { semaphore_.Signal(); }
	
private:
	v8::base::Semaphore semaphore_;
	v8::Isolate* isolate_;
};

static void StartSendingCommands(const v8::FunctionCallbackInfo<v8::Value>& info) {
	send_command_thread_->StartSending();
}


void bla() {
	DebugLocalContext env;
	v8::Isolate* isolate = env->GetIsolate();
	v8::HandleScope scope(isolate);
	v8::Local<v8::Context> context = env.context();
	
	counting_message_handler_counter = 0;
	
	v8::Debug::SetMessageHandler(
								 isolate, SendCommandThread::CountingAndSignallingMessageHandler);
	send_command_thread_ = new SendCommandThread(isolate);
	send_command_thread_->Start();
	
	v8::Local<v8::FunctionTemplate> start =
	v8::FunctionTemplate::New(isolate, StartSendingCommands);
	CHECK(env->Global()
		  ->Set(context, v8_str("start"),
				start->GetFunction(context).ToLocalChecked())
		  .FromJust());
	
	CompileRun("start(); while (true) { }");
	
	CHECK_EQ(20, counting_message_handler_counter);
	
	v8::Debug::SetMessageHandler(isolate, nullptr);
	CheckDebuggerUnloaded(isolate);
}
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#ifdef _WIN32
#include <winsock.h>
#include <io.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

#include <string>
#include <stdexcept>

std::vector<uint16_t> utf8_to_utf16(const std::string& utf8)
{
	std::vector<uint16_t> unicode;
	size_t i = 0;
	while (i < utf8.size())
	{
		unsigned long uni;
		size_t todo;
		bool error = false;
		unsigned char ch = utf8[i++];
		if (ch <= 0x7F)
		{
			uni = ch;
			todo = 0;
		}
		else if (ch <= 0xBF)
		{
			throw std::logic_error("not a UTF-8 string");
		}
		else if (ch <= 0xDF)
		{
			uni = ch&0x1F;
			todo = 1;
		}
		else if (ch <= 0xEF)
		{
			uni = ch&0x0F;
			todo = 2;
		}
		else if (ch <= 0xF7)
		{
			uni = ch&0x07;
			todo = 3;
		}
		else
		{
			throw std::logic_error("not a UTF-8 string");
		}
		for (size_t j = 0; j < todo; ++j)
		{
			if (i == utf8.size())
				throw std::logic_error("not a UTF-8 string");
			unsigned char ch = utf8[i++];
			if (ch < 0x80 || ch > 0xBF)
				throw std::logic_error("not a UTF-8 string");
			uni <<= 6;
			uni += ch & 0x3F;
		}
		if (uni >= 0xD800 && uni <= 0xDFFF)
			throw std::logic_error("not a UTF-8 string");
		if (uni > 0x10FFFF)
			throw std::logic_error("not a UTF-8 string");
		unicode.push_back(uni);
	}
	/*std::wstring utf16;
	for (size_t i = 0; i < unicode.size(); ++i)
	{
		unsigned long uni = unicode[i];
		if (uni <= 0xFFFF)
		{
			utf16 += (wchar_t)uni;
		}
		else
		{
			uni -= 0x10000;
			utf16 += (wchar_t)((uni >> 10) + 0xD800);
			utf16 += (wchar_t)((uni & 0x3FF) + 0xDC00);
		}
	}
	return utf16;*/
	return unicode;
}

void startserver(v8::Isolate* isolate);

namespace {
	int client_socket;
	
	void messageHandler(const v8::Debug::Message& message) {
		if (message.IsResponse()) {
			int a = 3;
			++a;
			//send_command_thread_->semaphore_.Signal();
			
			v8::Local<v8::String> string = message.GetJSON();
			v8::String::Utf8Value data(string);
			
			printf("Sending response: %s", *data);
			
			send(client_socket, *data, data.length(), 0);
		}
	}
	
	void run(void* isolate) {
		startserver((v8::Isolate*)isolate);
		/*Kore::Socket socket;
		socket.open(9911);
		const int maxsize = 512;
		unsigned char data[maxsize];
		unsigned fromAddress;
		unsigned fromPort;
		for (;;) {
			int size = socket.receive(data, maxsize, fromAddress, fromPort);
			if (size > 0) {
				v8::Debug::SendCommand((v8::Isolate*)isolate, (uint16_t*)data, size);
			}
		}*/
	}
}

void startDebugger(v8::Isolate* isolate) {
	v8::HandleScope scope(isolate);
	v8::Debug::SetMessageHandler(isolate, messageHandler);
	Kore::createAndRunThread(run, isolate);
}

#define PORT 9911
#define RCVBUFSIZE 1024

static void error_exit(const char *errorMessage);

#ifdef _WIN32
static void echo(v8::Isolate* isolate, SOCKET client_socket)
#else
static void echo(v8::Isolate* isolate, int client_socket)
#endif
{
	::client_socket = client_socket;
	char echo_buffer[RCVBUFSIZE];
	int recv_size;
	time_t zeit;
	
	if ((recv_size = recv(client_socket, echo_buffer, RCVBUFSIZE,0)) < 0) error_exit("recv() error");
	echo_buffer[recv_size] = '\0';
	time(&zeit);
	printf("Client Message: %s \t%s", echo_buffer, ctime(&zeit));
	
	int first_bracket = 0;
	for (int i = 0; i < recv_size; ++i) {
		if (echo_buffer[i] == '{') {
			first_bracket = i;
			break;
		}
	}
	
	v8::Local<v8::String> string = v8::String::NewFromUtf8(isolate, echo_buffer);
	//v8::String::Value value(string);
	
	char* json = &echo_buffer[first_bracket];
	std::vector<uint16_t> value = utf8_to_utf16(json);
	
	v8::Debug::SendCommand(isolate, value.data(), value.size());
}

static void error_exit(const char *error_message) {
#ifdef _WIN32
	fprintf(stderr,"%s: %d\n", error_message, WSAGetLastError());
#else
	fprintf(stderr, "%s: %s\n", error_message, strerror(errno));
#endif
	exit(EXIT_FAILURE);
}

void startserver(v8::Isolate* isolate) {
	struct sockaddr_in server, client;
	
#ifdef _WIN32
	SOCKET sock, fd;
#else
	int sock, fd;
#endif
	
	unsigned int len;
	
#ifdef _WIN32
	WORD wVersionRequested;
	WSADATA wsaData;
	wVersionRequested = MAKEWORD (1, 1);
	if (WSAStartup (wVersionRequested, &wsaData) != 0) error_exit("Winsock initialization error");
#endif
	
	sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0) error_exit("Socket error");
	
	memset(&server, 0, sizeof (server));
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	server.sin_port = htons(PORT);
	
	if(bind(sock,(struct sockaddr*)&server, sizeof( server)) < 0)
		error_exit("Could not bind socket");
	
	if (listen(sock, 5) == -1) error_exit("listen() error");
	
	printf("Server started\n");
	for (;;) {
		len = sizeof(client);
		fd = accept(sock, (struct sockaddr*)&client, &len);
		if (fd < 0) error_exit("accept() error");
		printf("Data from address: %s\n", inet_ntoa(client.sin_addr));
		echo(isolate, fd);
		
#ifdef _WIN32
		closesocket(fd);
#else
		close(fd);
#endif
	}
}
