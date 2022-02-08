/*  Ping Project.  */

/*  ����������� ���������.  */
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iostream>
#include <string>
#include <intrin.h>
#include <chrono>
#include <time.h>

/*  ������ ����������.  */
#pragma comment(lib, "ws2_32.lib")

using namespace std;

/*  ���������� ���������� � ���������.  */

SOCKET sock = INVALID_SOCKET;
struct sockaddr_storage t_addr;
struct addrinfo *test_addr = NULL;
struct addrinfo hints;
WSADATA wsaData = { 0 };
int iResult = 0;

struct sockaddr s_address;

//  ��������� ����������� ICMP-�����.
struct icmp_packet
{
	//  ��������� ������.
	char type;  //  ��� ���������, 0 ��� 8.
	char code;  //  ��� ������.
	short control_sum;
	short id;
	short seq;  //  �������.

	//  ������ ������.
	const char data[33] = "abcdefghijklmnopqrstuvwabcdefghi";
};


/*  ��������������� ��������.  */

/*  ������� �������� int � �������� string.  */
string to_binary_string(unsigned int n)
{
	string buffer; // ������� ������ � �������� �������
	// ������� ������ ������� �� ���������
	buffer.reserve(numeric_limits<unsigned int>::digits);
	do
	{
		buffer += char('0' + n % 2); // ��������� � �����
		n = n / 2;
	} while (n > 0);
	buffer = string(buffer.crbegin(), buffer.crend());
	while (buffer.length() < 8)
		buffer = '0' + buffer;
	return buffer; // ������������� ���������
}

/*  ������� �������� �� ��������� �������� string � ���������� short.  */
short bin_to_short(string value)
{
	short result = 0;
	short mnozh = 1;
	for (int i = value.length() - 1; i > 0; i--)
	{
		result += (value[i] - 48)* mnozh;
		mnozh *= 2;
	}
	return result;
}

/*  ������� �������� ����������� �����.  */
int control_sum(icmp_packet packet)
{
	int sum = 0;

	//  ����������� ���� type&code � 16 ���.
	string pair = to_binary_string(packet.type) + to_binary_string(packet.code);
	sum += bin_to_short(pair);
	sum += _byteswap_ushort(packet.id);  // 0x0100 -> 0x0001
	sum += _byteswap_ushort(packet.seq);

	pair = "";
	//  ����������� ��� �� �������� ���� ������.
	for (int i = 0; i < 32; i++)
	{
		pair += to_binary_string(packet.data[i]);
		
		if ((i % 2) != 0)
		{
			sum += bin_to_short(pair);
			pair = "";
		}
	}
	
	//  ������������� ���������� ��� 16 ���.
	while (sum > 0xffff)
		sum = (sum & 0xffff) + (sum >> 16);

	return _byteswap_ushort(~sum);
}

/*  ������� ������������ ICMP ������.  */
icmp_packet fill_icmp_packet()
{
	icmp_packet packet;
	packet.type = 8;
	packet.code = 0;
	packet.control_sum = 0;
	packet.id = 0x0100;
	packet.seq = 0x5407;

	return packet;
}

/*  ������� �������������� char = 8 => string = "8".  */
string ch_to_str(unsigned char val)
{
	string res = "";
	char chislo;

	//  ��������� ������.
	chislo = val % 10;
	val = val - chislo;
	chislo += 48;
	if (chislo != '0')
		res = res + chislo;

	//  ��������� ��������.
	chislo = (val % 100);
	val = val - chislo;
	chislo /= 10;
	chislo += 48;
	if (chislo != '0')
		res = chislo + res;

	//  ��������� �����.
	chislo = (val % 1000);
	chislo /= 100;
	chislo += 48;
	if (chislo != '0')
		res = chislo + res;

	return res;
}

/*  ������� �������������� ������ � ���� ������� �� 4-� ���������  */
unsigned char* ip_to_array(string ip, unsigned char* arr)
{
	unsigned char val = 0;
	short cntr = 0;

	for (int i = 0; i < ip.length(); i++)
	{
		if ((ip[i] > 47) && (ip[i] < 58))
			val = (val * 10) + ((int)ip[i]) - 48;
		else
		{
			arr[cntr] = (char)val;  //  ������� ���������� ��������� ip �������.
			val = 0;
			cntr++;
		}
	}
	arr[cntr] = (char)val;

	return arr;
}

/*  ������� �������������� ip �� ��������� SOCKADDR_STORAGE � string.  */
string ip_to_str(SOCKADDR_STORAGE ip)
{
	string res = "";
	
	for (int i = 2; i < 6; i++)
	{
		res += ch_to_str((unsigned char)ip.__ss_pad1[i]);
		if ((i + 1) < 6)
			res += '.';
	}

	return res;
}

/*  ������� �������� ICMP ������.  */
int send_packet(icmp_packet packet, int *stat_array)
{
	char buf[128];
	int timeout = 5000;
	int t_addr_size = sizeof(t_addr);
	char time_buf[256];  //  ���������� ������ ��� ������ ������.
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(int));  //  ��������� ����-���� (5000 ��.).

	auto start = std::chrono::high_resolution_clock::now();

	//  �������� ������.
	int send_res = sendto(sock, (char*)&packet, 40, 0, (SOCKADDR*)&t_addr, sizeof(t_addr));
	if (send_res == SOCKET_ERROR)
	{
		cout << "Packet sending failed with " << GetLastError() << " error code." << endl;
		closesocket(sock);
		WSACleanup();
		return 1;
	}
	cout << "Packet with " << send_res - 8 << " bytes was sent success." << endl;
	stat_array[0] += 1;  //  +1 ������������ �����.

	//  ��������� ������.
	int recv_res = recvfrom(sock, (char*)&buf, (int)strlen(buf), NULL, (SOCKADDR*)&t_addr, &t_addr_size);
	if (GetLastError() == 10060)
	{
		cout << "Request waiting interval exceeded." << endl;
		stat_array[2] += 1;  //  +1 ���������� �����.
	}
	else if (recv_res == SOCKET_ERROR)
	{
		if (recv_res == 0)
			cout << "The connection was closed." << endl;
		else
			cout << "Data receiving failed with " << GetLastError() << endl;
		closesocket(sock);
		WSACleanup();
		return 1;
	}

	auto end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> elapsed_time = end - start;  //  ���������� ������������ �� ��������� ������ �������.
	if (recv_res > 0)
	{
		cout << "Recevied " << recv_res - 28 << " bytes from " << ip_to_str(t_addr) << " in " << (int)(elapsed_time.count() * 1000) << " ms." << endl;

		//  ���� ����������.
		stat_array[1] += 1;  //  +1 ���������� �����.
		if (stat_array[3] > (int)(elapsed_time.count() * 1000))  //  ����������� ����� ��������� ������.
			stat_array[3] = (int)(elapsed_time.count() * 1000);
		if (stat_array[4] < (int)(elapsed_time.count() * 1000))  //  ������������ ����� ��������� ������.
			stat_array[4] = (int)(elapsed_time.count() * 1000);
		stat_array[5] += (int)(elapsed_time.count() * 1000);  //  ����� ������� �������� ���� ���������
															  //  (��� ���������� �������� �������).
	}

	return 0;
}

/*  ������� �������� � ��������� ���������� ������.  */
int run_socket(string ip_address)
{
	//  ������ ������ � �����.
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		wprintf(L"WSAStartup failed: %d\n", iResult);
		return 1;
	}

	//  ���������� ��������� �������� ���������� � ������.
	if ((ip_address[0] > 47) && (ip_address[0] < 58))  //  ���� ������ ip �����.
	{
		unsigned char ip_array[4];

		ip_to_array(ip_address, ip_array);
		t_addr.ss_family = AF_INET;
		t_addr.__ss_pad1[2] = ip_array[0];
		t_addr.__ss_pad1[3] = ip_array[1];
		t_addr.__ss_pad1[4] = ip_array[2];
		t_addr.__ss_pad1[5] = ip_array[3];
	}
	else  //  ���� ������ �����.
	{
		//  ��������� ip ������ �� ����� �����.
		auto getaddr_res = getaddrinfo("google.com", NULL, &hints, &test_addr);  //  ����� �������� � test_addr.
		if (getaddr_res != 0)
		{
			cout << "Getaddrinfo function failed." << endl;
			closesocket(sock);
			WSACleanup();
			return 1;
		}

		//  ����������� ����������� ip �� ��������� addrinfo* � sockaddr_storage.
		for (int i = 2; i < 6; i++)
			t_addr.__ss_pad1[i] = test_addr->ai_addr->sa_data[i];
		t_addr.ss_family = AF_INET;
	}

	//  ������������� ������.
	sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (sock == INVALID_SOCKET)
	{
		cout << "Socket creation failed." << endl;
		WSACleanup();
		return 1;
	}

	//  ��������� ���������� �� ������.
	if (connect(sock, (sockaddr*) &t_addr, sizeof(t_addr)) == SOCKET_ERROR)
	{
		cout << "Connection to " << ip_address << " failed." << endl;
		closesocket(sock);
		WSACleanup();
		return 1;
	}
	cout << "Connection to " << ip_address << " success.\nStarting send echo request..." << endl;

	return 0;
}




/*  �������� ������� main.  */
int main()
{
	int stat_array[6];
	stat_array[0] = 0;
	stat_array[1] = 0;
	stat_array[2] = 0;
	stat_array[3] = 999;
	stat_array[4] = 0;
	stat_array[5] = 0;
	int ping_count;
	string ip_addr;

	//  ���������� ������ ���������������� �������.
	icmp_packet packet = fill_icmp_packet();
	packet.control_sum = control_sum(packet);  //  ������� ����������� �����

	cout << "Ping program.\n" << endl;
	cout << "Enter the IP-Address or domain name to connect." << endl;
	
	getline(cin, ip_addr);

	cout << "How many times you want to ping?" << endl;
	
	cin >> ping_count;
	cout << endl;

	if (ping_count <= 0)
		ping_count = 1;

	//  ������ ������ ������.
	run_socket(ip_addr);

	//  �������� �������.

	for (int i = 0; i < ping_count; i++)
	{
		send_packet(packet, stat_array);
		packet.seq += _byteswap_ushort(1);
		packet.control_sum = 0;
		packet.control_sum = control_sum(packet);
	}

	stat_array[5] /= ping_count;  //  ���������� �������� ������� ��������� ������.

	cout << "\nStatistics.\n\tPackets sent:\t" << stat_array[0] << "\n\tPackets received:\t" << stat_array[1] << "\n\tPacket loss:\t" << stat_array[2];
	cout << " (" << stat_array[2] * (100 / stat_array[0]) << "%)" << endl;
	cout << "\nSend and recieve time.\n\tMin = " << stat_array[3] << " ms.\n\tMax = " << stat_array[4] << " ms.";
	cout << "\n\tAverage = " << stat_array[5] << " ms." << endl;

	closesocket(sock);
	system("pause");
	return 0;
}