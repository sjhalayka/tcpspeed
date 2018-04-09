#include <winsock2.h>
#include <windows.h>
#pragma comment(lib, "ws2_32")
#include <iostream>
#include <string>
#include <sstream>
#include <cstring>
using std::cout;
using std::endl;
using std::string;
using std::istringstream;
using std::ios;

bool stop = false;
SOCKET tcp_socket = INVALID_SOCKET;
enum program_mode { talk_mode, listen_mode };

void print_usage(void)
{
	cout << "  USAGE:" << endl;
	cout << "    Listen mode:" << endl;
	cout << "      tcpspeed PORT_NUMBER" << endl;
	cout << endl;
	cout << "    Talk mode:" << endl;
	cout << "      tcpspeed TARGET_HOST PORT_NUMBER" << endl;
	cout << endl;
	cout << "    ie:" << endl;
	cout << "      Listen mode: tcpspeed 1920" << endl;
	cout << "      Talk mode:   tcpspeed www 342" << endl;
	cout << "      Talk mode:   tcpspeed 10.200.67.1 950" << endl;
	cout << endl;
}

bool verify_port(const string &port_string, unsigned long int &port_number)
{
	for (size_t i = 0; i < port_string.length(); i++)
	{
		if (!isdigit(port_string[i]))
		{
			cout << "  Invalid port: " << port_string << endl;
			cout << "  Ports are specified by numerals only." << endl;
			return false;
		}
	}

	istringstream iss(port_string);
	iss >> port_number;
	
	if (port_string.length() > 5 || port_number > 65535 || port_number == 0)
	{
		cout << "  Invalid port: " << port_string << endl;
		cout << "  Port must be in the range of 1-65535" << endl;
		return false;
	}
	
	return true;
}

bool init_winsock(void)
{
	WSADATA wsa_data;
	WORD ver_requested = MAKEWORD(2, 2);

	if (WSAStartup(ver_requested, &wsa_data))
	{
		cout << "Could not initialize Winsock 2.2.";
		return false;
	}

	if (LOBYTE(wsa_data.wVersion) != 2 || HIBYTE(wsa_data.wVersion) != 2)
	{
		cout << "Required version of Winsock (2.2) not available.";
		return false;
	}

	return true;
}

BOOL console_control_handler(DWORD control_type)
{
	stop = true;
	closesocket(tcp_socket);
	return TRUE;
}

bool init_options(const int &argc, char **argv, enum program_mode &mode, string &target_host_string, long unsigned int &port_number)
{
	if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)console_control_handler, TRUE))
	{
		cout << "  Could not add console control handler." << endl;
		return false;
	}

	if (!init_winsock())
		return false;
	
	string port_string = "";
	
	if (2 == argc)
	{
		mode = listen_mode;
		port_string = argv[1];
	}
	else if (3 == argc)
	{
		mode = talk_mode;
		target_host_string = argv[1];
		port_string = argv[2];
	}
	else
	{
		print_usage();
		return false;
	}
	
	cout.setf(ios::fixed, ios::floatfield);
	cout.precision(2);
	
	return verify_port(port_string, port_number);
}

void cleanup(void)
{
	// if the program was aborted, flush cout and print a final goodbye
	if (stop)
	{
		cout.flush();
		cout << endl << "  Stopping." << endl;
	}

	// if the socket is still open, close it
	if (INVALID_SOCKET != tcp_socket)
		closesocket(tcp_socket);

	// shut down winsock
	WSACleanup();

	// remove the console control handler
	SetConsoleCtrlHandler((PHANDLER_ROUTINE)console_control_handler, FALSE);
}

int main(int argc, char **argv)
{
	cout << endl << "tcpspeed 1.1 - TCP speed tester" << endl << "Copyright 2018, Shawn Halayka" << endl << endl;

	program_mode mode = listen_mode;
	string target_host_string = "";
	long unsigned int port_number = 0;
	const long unsigned int tx_buf_size = 1450;
	char tx_buf[1450];
	const long unsigned int rx_buf_size = 8196;
	char rx_buf[8196];

	// initialize winsock and all of the program's options
	if (!init_options(argc, argv, mode, target_host_string, port_number))
	{
		cleanup();
		return 1;
	}

	if (talk_mode == mode)
	{
		cout << "  Talking on TCP port " << port_number << " - CTRL+C to exit." << endl;

		struct sockaddr_in their_addr;
		struct hostent *he = gethostbyname(target_host_string.c_str());
		
		if (NULL == he)
		{
			cout << "  Could not resolve target host." << endl;
			cleanup();
			return 2;
		}

		their_addr.sin_family = AF_INET;
		their_addr.sin_port = htons((unsigned short int)port_number);
		their_addr.sin_addr = *((struct in_addr *)he->h_addr);
		memset(&(their_addr.sin_zero), '\0', 8);

		if (INVALID_SOCKET == (tcp_socket = socket(AF_INET, SOCK_STREAM, 0)))
		{
			cout << "  Could not allocate a new socket." << endl;
			cleanup();
			return 3;
		}

		if (SOCKET_ERROR == connect(tcp_socket, (struct sockaddr *)&their_addr, sizeof(struct sockaddr)))
		{
			cout << "  Connect error." << endl;
			cleanup();
			return 3;
		}

		while (!stop)
		{
			if (SOCKET_ERROR == (send(tcp_socket, tx_buf, tx_buf_size, 0)))
			{
				if (!stop)
					cout << "  Send error." << endl;

				break;
			}
		}
	}
	else if (listen_mode == mode)
	{
		cout << "  Listening on TCP port " << port_number << " - CTRL+C to exit." << endl;

		struct sockaddr_in my_addr;
		int sock_addr_len = sizeof(struct sockaddr);
		my_addr.sin_family = AF_INET;
		my_addr.sin_port = htons((unsigned short int)port_number);
		my_addr.sin_addr.s_addr = INADDR_ANY;
		memset(&(my_addr.sin_zero), '\0', 8);

		if (INVALID_SOCKET == (tcp_socket = socket(AF_INET, SOCK_STREAM, 0)))
		{
			cout << "  Could not allocate a new socket." << endl;
			cleanup();
			return 4;
		}

		if (SOCKET_ERROR == bind(tcp_socket, (struct sockaddr *) &my_addr, sizeof(struct sockaddr)))
		{
			cout << "  Could not bind socket to port " << port_number << "." << endl;
			cleanup();
			return 5;
		}

		if (SOCKET_ERROR == listen(tcp_socket, 0))
		{
			cout << "  Listen error." << endl;
			cleanup();
			return 6;
		}

		SOCKET accept_socket = INVALID_SOCKET;

		if (INVALID_SOCKET == (accept_socket = accept(tcp_socket, (struct sockaddr *) &my_addr, &sock_addr_len)))
		{
			cout << "  Accept error." << endl;
			cleanup();
			return 7;
		}

		long unsigned int start_loop_ticks = 0;
		long unsigned int end_loop_ticks = 0;
		long unsigned int elapsed_loop_ticks = 0;
		__int64 total_elapsed_ticks = 0;
		__int64 total_bytes_received = 0;
		__int64 last_reported_at_ticks = 0;
		__int64 last_reported_total_bytes_received = 0;

		double record_bps = 0;
		long unsigned int temp_bytes_received = 0;

		while (!stop)
		{
			start_loop_ticks = GetTickCount();

			if (SOCKET_ERROR == (temp_bytes_received = recv(accept_socket, rx_buf, rx_buf_size, 0)))
			{
				if (!stop)
				{
					cout << "  Receive error." << endl;
					cleanup();
					return 6;
				}
			}
			else
			{
				total_bytes_received += temp_bytes_received;
			}


			end_loop_ticks = GetTickCount();

			if (end_loop_ticks < start_loop_ticks)
				elapsed_loop_ticks = MAXDWORD - start_loop_ticks + end_loop_ticks;
			else
				elapsed_loop_ticks = end_loop_ticks - start_loop_ticks;

			total_elapsed_ticks += elapsed_loop_ticks;

			if (total_elapsed_ticks >= last_reported_at_ticks + 1000)
			{
				__int64 bytes_sent_received_between_reports = total_bytes_received - last_reported_total_bytes_received;

				double bytes_per_second = static_cast<double>(bytes_sent_received_between_reports) / ((static_cast<double>(total_elapsed_ticks) - static_cast<double>(last_reported_at_ticks)) / 1000.0);

				if (bytes_per_second > record_bps)
					record_bps = bytes_per_second;

				last_reported_at_ticks = total_elapsed_ticks;
				last_reported_total_bytes_received = total_bytes_received;

				static const double mbits_factor = 8.0 / (1024.0 * 1024);
				cout << "  " << bytes_per_second * mbits_factor << " Mbit/s, Record: " << record_bps * mbits_factor << " Mbit/s" << endl;
			}
		}
	}

	cleanup();
	return 0;
}
