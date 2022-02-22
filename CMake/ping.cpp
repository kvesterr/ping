/*  Ping Project.  */

/*  Подключение библиотек.  */
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iostream>
#include <string>
#include <intrin.h>
#include <chrono>
#include <time.h>
#include <conio.h>
#include <thread>
#include <fstream>
#include <direct.h>

/*  Прочие объявления.  */
#pragma comment(lib, "ws2_32.lib")

using namespace std;

/*  Глобальные переменные и константы.  */

SOCKET sock = INVALID_SOCKET;
struct sockaddr_storage t_addr;
struct addrinfo* test_addr = NULL;
struct addrinfo hints;
WSADATA wsaData = { 0 };
int iResult = 0;
bool ping_was_stopped = false;

struct sockaddr s_address;

//  Структура описывающая ICMP-пакет.
struct icmp_packet
{
	//  Заголовок пакета.
	char type;  //  Тип сообщения, 0 или 8.
	char code;  //  Код ошибки.
	short control_sum;
	short id;
	short seq;  //  Очередь.

	//  Данные пакета.
	const char data[33] = "abcdefghijklmnopqrstuvwabcdefghi";
};


/*  Вспомогательные функцкии.  */

/*  Функция записи событий в лог-файл.  */
int write_log(bool type, int error_code, string description)
{
	ofstream log_file;

	//  Открытие файла.
	log_file.open("res\\log.txt", ios_base::app);  //  Открытие с добавлением в конец строки.
	if (!log_file.is_open())
	{
		_mkdir("res");
		log_file.open("res\\log.txt", ios_base::app);
	}

	//  Запись в файл.
	__time64_t now;
	struct tm timeinfo;
	char buf[32];
	string s_type = "";
	if (type == 0)
		s_type = "operation";
	else
		s_type = "error";

	//  Запись в buf текущей даты и времени.
	now = time(NULL);
	_localtime64_s(&timeinfo, &now);
	asctime_s(buf, &timeinfo);
	string s_buf = buf;

	size_t found = s_buf.find("\n");
	s_buf.replace(found, 1, "");
	s_buf = "[" + s_buf + "]" + "  \t" + "[" + s_type + "]" + "  \t" + "code: " + std::to_string(error_code) + "  \t" + "description: " + description + ".\n";

	//  Запись данных.
	log_file << s_buf.c_str();

	log_file.close();

	return 0;
}

/*  Функция для потока, для выхода из программы.  */
void f_for_thread()
{
	while (!ping_was_stopped)
	{
		Sleep(500);
		if (_getch() == VK_TAB)  //  Условие нажатия TAB.
		{
			ping_was_stopped = true;
			system("pause>0");  //  Пауза без надписи "для продолжения нажмите любую клавишу".
			exit(0);
		}
	}
}

/*  Функция перевода int в двоичный string.  */
string to_binary_string(unsigned int n)
{
	string buffer; // символы ответа в обратном порядке
	// выделим память заранее по максимуму
	buffer.reserve(numeric_limits<unsigned int>::digits);
	do
	{
		buffer += char('0' + n % 2); // добавляем в конец
		n = n / 2;
	} while (n > 0);
	buffer = string(buffer.crbegin(), buffer.crend());
	while (buffer.length() < 8)
		buffer = '0' + buffer;
	return buffer; // разворачиваем результат
}

/*  Функция перевода из двоичного значения string в десятичное short.  */
short bin_to_short(string value)
{
	short result = 0;
	short mnozh = 1;
	for (size_t i = value.length() - 1; i > 0; i--)
	{
		result += (value[i] - 48) * mnozh;
		mnozh *= 2;
	}
	return result;
}

/*  Функция подсчета контрольной суммы.  */
int control_sum(icmp_packet packet)
{
	int sum = 0;

	//  Составление пары type&code в 16 бит.
	string pair = to_binary_string(packet.type) + to_binary_string(packet.code);
	sum += bin_to_short(pair);
	sum += _byteswap_ushort(packet.id);  // 0x0100 -> 0x0001
	sum += _byteswap_ushort(packet.seq);

	pair = "";
	//  Составление пар из значений поля данных.
	for (int i = 0; i < 32; i++)
	{
		pair += to_binary_string(packet.data[i]);

		if ((i % 2) != 0)
		{
			sum += bin_to_short(pair);
			pair = "";
		}
	}

	//  Корректировка результата под 16 бит.
	while (sum > 0xffff)
		sum = (sum & 0xffff) + (sum >> 16);

	return _byteswap_ushort(~sum);
}

/*  Функция формирования ICMP пакета.  */
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

/*  Функция преобразования char = 8 => string = "8".  */
string ch_to_str(unsigned char val)
{
	string res = "";
	char chislo;

	//  Обработка едениц.
	chislo = val % 10;
	val = val - chislo;
	chislo += 48;
	if (chislo != '0')
		res = res + chislo;

	//  Обработка десятков.
	chislo = (val % 100);
	val = val - chislo;
	chislo /= 10;
	chislo += 48;
	if (chislo != '0')
		res = chislo + res;

	//  Обработка сотен.
	chislo = (val % 1000);
	chislo /= 100;
	chislo += 48;
	if (chislo != '0')
		res = chislo + res;

	return res;
}

/*  Функция представляющая строку ip в виде массива из 4-х элементов  */
char* ip_to_array(string ip, char* arr)
{
	char val = 0;
	short cntr = 0;

	for (int i = 0; i < ip.length(); i++)
	{
		if ((ip[i] > 47) && (ip[i] < 58))  //  Собираем символы цифр в число.
			val = (val * 10) + ((int)ip[i]) - 48;
		else
		{
			arr[cntr] = val;
			val = 0;
			cntr++;
		}
	}
	arr[cntr] = val;

	return arr;
}

/*  Функция преобразования ip из структуры SOCKADDR_STORAGE в string.  */
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

/*  Функция отправки ICMP пакета.  */
int send_packet(icmp_packet packet, int* stat_array)
{
	if (ping_was_stopped)
		return -1;

	char buf[1024];
	int timeout = 5000;
	int t_addr_size = sizeof(t_addr);
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(int));  //  Установка тайм-аута (5000 мс.).

	auto start = std::chrono::high_resolution_clock::now();

	//  Отправка пакета.
	int send_res = sendto(sock, (char*)&packet, 40, 0, (SOCKADDR*)&t_addr, sizeof(t_addr));
	if (send_res == SOCKET_ERROR)
	{
		cout << "Packet sending failed with " << GetLastError() << " error code." << endl;
		write_log(1, GetLastError(), "Packet sending fail");
		closesocket(sock);
		WSACleanup();
		write_log(0, 1, "Ping program end");
		return 1;
	}
	cout << "Packet with " << send_res - 8 << " bytes was sent success." << endl;
	write_log(0, 0, "Packet sending success");
	stat_array[0] += 1;  //  +1 отправленный пакет.

	//  Получение ответа.
	int recv_res = recvfrom(sock, (char*)&buf, (int)strlen(buf), NULL, (SOCKADDR*)&t_addr, &t_addr_size);
	if (GetLastError() == 10060)
	{
		cout << "Request waiting interval exceeded." << endl;
		write_log(1, 10060, "Request waiting interval exceeded");
		stat_array[2] += 1;  //  +1 потерянный пакет.
	}
	else if (recv_res == SOCKET_ERROR)
	{
		if (recv_res == 0)
		{
			cout << "The connection was closed." << endl;
			write_log(0, 0, "The connection was closed");
		}
		else
		{
			cout << "Data receiving failed with " << GetLastError() << endl;
			write_log(1, GetLastError(), "Data receiving failed");
		}
		closesocket(sock);
		WSACleanup();
		write_log(0, 1, "Ping program end");
		return 1;
	}

	auto end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> elapsed_time = end - start;  //  Вычисление затраченного на получение пакета времени.
	if (recv_res > 0)
	{
		cout << "Recevied " << recv_res - 28 << " bytes from " << ip_to_str(t_addr) << " in " << (int)(elapsed_time.count() * 1000) << " ms." << endl;
		write_log(0, 0, "Packet receiving success");

		//  Сбор статистики.
		stat_array[1] += 1;  //  +1 полученный пакет.
		if (stat_array[3] > (int)(elapsed_time.count() * 1000))  //  Минимальное время получения ответа.
			stat_array[3] = (int)(elapsed_time.count() * 1000);
		if (stat_array[4] < (int)(elapsed_time.count() * 1000))  //  Максимальное время получения ответа.
			stat_array[4] = (int)(elapsed_time.count() * 1000);
		stat_array[5] += (int)(elapsed_time.count() * 1000);  //  Сумма времени отправки всех сообщений
															  //  (для вычисления среднего времени).
	}

	return 0;
}

/*  Функция проверки ip адреса в виде строки на корректность.  */
bool is_ip_valid(string ip)
{
	byte point_count = 0;  //  Переменная для хранения количества точек в адресе.
	char arr[4];
	string source_ip = ip;

	//  Проверка на количество точек в адресе (должно быть 3).
	size_t found = ip.find(".");
	while (found != string::npos)
	{
		ip.replace(found, 1, "");
		point_count++;
		found = ip.find(".");
	}

	if (point_count != 3)
		return false;


	//  Проверка, являются ли все символы строки цифрами.
	for (int i = 0; i < ip.length(); i++)
		if (!((ip[i] > 47) && (ip[i] < 58)))
			return false;

	//  Проверка каждого числа в ip адресе (0 >= chislo < 256).
	ip_to_array(source_ip, arr);
	for (int i = 0; i < 4; i++)
	{
		int value = arr[i];
		if (value < 0)
			value += 256;
		if ((value < 0) || (value > 255))
			return false;
	}

	return true;
}

/*  Функция создания и установки соеденения сокета.  */
int run_socket(string ip_address)
{
	//  Начало работы с сетью.
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		wprintf(L"WSAStartup failed: %d\n", iResult);
		write_log(1, iResult, "WSAStartup failed");
		write_log(0, 1, "Ping program end");
		return 1;
	}
	write_log(0, 0, "WSAStartup success");

	//  Заполнение структуры хранящей информацию о адресе.
	if ((ip_address[0] > 47) && (ip_address[0] < 58))  //  Если введен ip адрес.
	{
		char ip_array[4];

		if (!is_ip_valid(ip_address))
		{
			cout << "IP address is incorrect." << endl;
			write_log(1, 421, "IP adress is incorrect");
			write_log(0, 1, "Ping program end");
			return 1;
		}

		ip_to_array(ip_address, ip_array);

		t_addr.ss_family = AF_INET;
		t_addr.__ss_pad1[2] = ip_array[0];
		t_addr.__ss_pad1[3] = ip_array[1];
		t_addr.__ss_pad1[4] = ip_array[2];
		t_addr.__ss_pad1[5] = ip_array[3];
	}
	else  //  Если введен домен.
	{
		//  Получение ip адреса по имени хоста.
		auto getaddr_res = getaddrinfo(ip_address.c_str(), NULL, &hints, &test_addr);  //  Адрес получаем в test_addr.
		if (getaddr_res != 0)
		{
			cout << "Unknown domain name." << endl;
			write_log(1, 422, "Unknown domain name");
			closesocket(sock);
			WSACleanup();
			write_log(0, 1, "Ping program end");
			return 1;
		}

		//  Копирование полученного ip из структуры addrinfo* в sockaddr_storage.
		for (int i = 2; i < 6; i++)
			t_addr.__ss_pad1[i] = test_addr->ai_addr->sa_data[i];
		t_addr.ss_family = AF_INET;
	}

	//  Инициализация сокета.
	sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (sock == INVALID_SOCKET)
	{
		cout << "Socket creation failed." << endl;
		write_log(1, 420, "Socket creation failed");
		WSACleanup();
		write_log(0, 1, "Ping program end");
		return 1;
	}

	//  Установка соеденения по сокету.
	if (connect(sock, (sockaddr*)&t_addr, sizeof(t_addr)) == SOCKET_ERROR)
	{
		cout << "Connection to " << ip_address << " failed." << endl;
		write_log(1, 423, "Connection to " + ip_address + " failed");
		closesocket(sock);
		WSACleanup();
		write_log(0, 1, "Ping program end");
		return 1;
	}
	cout << "Connection to " << ip_address << " success." << endl;
	write_log(0, 0, "Connection to " + ip_address + " success");

	return 0;
}

/*  Основная функция main.  */
int main()
{
	write_log(0, 0, "Ping program start");
	int stat_array[6];
	stat_array[0] = 0;
	stat_array[1] = 0;
	stat_array[2] = 0;
	stat_array[3] = 999;
	stat_array[4] = 0;
	stat_array[5] = 0;
	int ping_count;
	int real_ping_count = 0;
	string ping_count_str;
	string ip_addr;

	//  Заполнение пакета соответствующими данными.
	icmp_packet packet = fill_icmp_packet();
	packet.control_sum = control_sum(packet);  //  Подсчет контрольной суммы

	cout << "Ping program.\n" << endl;
	cout << "Enter the IP-Address or domain name to connect." << endl;

	getline(cin, ip_addr);

	//  Начало работы сокета.
	if (run_socket(ip_addr) != 0)
	{
		write_log(1, 425, "Run socket unknown error");
		write_log(0, 1, "Ping program end");
		system("pause");
		return 1;
	}

	cout << "How many times you want to ping?" << endl;
	getline(cin, ping_count_str);

	cout << endl;
	if ((ping_count_str != "") && (ping_count_str.length() < 10))  //  Если пользователь ввел сколько раз нужно отправить пакеты.
	{
		try
		{
			ping_count = stoi(ping_count_str);
		}
		catch (exception e)
		{
			ping_count = 1;
		}
	}
	else  //  Если оставил поле сколько раз нужно отправить пакеты пустым.
		ping_count = 1;

	if ((ping_count <= 0) || (ping_count > 5000))
		ping_count = 1;


	//  Отправка пакетов.
	thread th(f_for_thread);
	cout << "Start sending echo-requests..." << endl;
	Sleep(500);

	for (int i = 0; i < ping_count; i++)
	{
		int send_res = send_packet(packet, stat_array);
		if (send_res == -1)  //  Если пинг завершен досрочно, нажатием клавиши TAB.
		{
			write_log(0, 1, "Pinging was stopped by user");
			break;
		}
		else if (send_res != 0)  //  Остальные случаи ошибки отправки пакета.
		{
			ping_was_stopped = true;
			th.join();
			write_log(1, 426, "Packet send unknown error");
			write_log(0, 1, "Ping program end");
			system("pause");
			return 1;
		}

		//  Увеличение значиния очереди и последующий подсчет новой контрольной суммы.
		//  _byteswap_ushort(): 0x0001 => 0x0100 (LE => BE).
		packet.seq += _byteswap_ushort(1);
		packet.control_sum = 0;
		packet.control_sum = control_sum(packet);

		real_ping_count++;
	}

	stat_array[5] /= real_ping_count;  //  Вычисление среднего времени получения пакета.

	//  Вывод статистики пинга.
	cout << "\nStatistics.\n\tPackets sent:\t" << stat_array[0] << "\n\tPackets received:\t" << stat_array[1] << "\n\tPacket loss:\t" << stat_array[2];
	cout << " (" << stat_array[2] * (100 / stat_array[0]) << "%)" << endl;
	if (stat_array[3] == 999)
		stat_array[3] = 0;
	cout << "\nSend and recieve time.\n\tMin = " << stat_array[3] << " ms.\n\tMax = " << stat_array[4] << " ms.";
	cout << "\n\tAverage = " << stat_array[5] << " ms." << endl;

	ping_was_stopped = true;

	closesocket(sock);
	write_log(0, 0, "Socket was closed");
	th.join();
	system("pause");
	write_log(0, 0, "Ping program end");

	return 0;
}
