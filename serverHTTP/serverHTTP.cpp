
#include <WinSock2.h>
#include <stdio.h>
#include <string.h>

#pragma warning(disable:4996)
#pragma comment(lib, "wsock32.lib") //необходимая библиотека для интерфейса сокетов

#define ROOT "public_html"  //корневая папка
#define HTTP_GET 1
#define HTTP_HEAD 2

char def_file[] = {"index.html"};
char error[] = {"HTTP/1.1 404 File not found\n\n"};
char ok[] = {"HTTP/1.1 200 OK\nServer: CNAI Web Server\nContent-Length: "};
char ctype[] = {"Content-Type: "};
char end[] = {"\n\n"};

//расширения поддерживаемых файлов
char types[][20] = {
	"png", "image/png",
	"gif", "image/gif",
	"pdf", "application/pdf",
	"docx", "application/MS_word",
	"css", "text/css",
	"html", "text/html",
	"", "text/unknown_type"}; // неопределенное расширение

//определение расширения (возвращает расширение из списка types)
char *get_type(char *fname)
{
	char *FExt = strrchr(fname, '.'); //последнее вхождение символа '.'
	for(int i = 0;; i += 2)
		//находим расширение из списка	types
		if(!types[i][0] || !strcmp(FExt + 1, types[i]))
			return types[i + 1];
}

//получаем пакет данных (возвращает размер пакета)
int recv_data(SOCKET s, char *data)
{
	int i, rv = NULL;
	char tmp[200] = {"\0"};
	do
	{
		i = 0;
		while(true)
		{
			// получаем строку запроса
			if(!recv(s, tmp + i, 1, 0))
				/* s -сокет-дескриптор, из которого читаются данные
				   tmp + i, 1 - адрес и длина буфера для записи читаемых данных
				   0 - комбинация битовых флагов, управляющих режимами чтения*/
				break;
			// конец пакета
			if(tmp[i] == '\n' && tmp[i - 1] == '\r')
				break;
			i++;
		}

		// запоминаем запрашиваемый файл
		if(!strncmp(tmp, "GET ", 4) || !strncmp(tmp, "HEAD ", 5))
		{
			rv = (*tmp == 'G') ? HTTP_GET : HTTP_HEAD;
			for(i = (rv == HTTP_GET) ? 4 : 5; !strchr(" ", tmp[i]); i++)
				if(tmp[i] == '/')
					tmp[i] = '\\'; //замена прямых слешей на обратные

			tmp[i] = '\0';
			strcpy(data, tmp + ((rv == HTTP_GET) ? 4 : 5));
		}
	} while(i > 1);
	return rv;
}

//отправляем данные
void send_data(SOCKET sock, char *file_name, int method)
{
	char file_path[300];
	FILE *file;
	char *data = NULL, *type;

	// конвертируем запрашиваемое имя файла в реальное
	if(!strcmp(file_name, "\\")) //корневой каталог,index.html
		sprintf(file_path, "%s\\%s", ROOT, def_file); //формирование пути к	файлу
	else
		sprintf(file_path, "%s%s", ROOT, file_name); //не корень

	printf("\n%s %s ", (method == HTTP_GET) ? "GET" : "HEAD", file_path);

	type = get_type(file_path); // проверяем расширение

	if(!(file = fopen(file_path, "rb"))) //Пытаемся открыть файл
	{//Если файл не найден
		printf("\n%s - NOT FOUND (HTTP/1.1 404)\n", file_path);
		send(sock, error, strlen(error), 0);
		return;
	}


	fseek(file, 0, SEEK_END);   // fseek перемещает указатель позиции в потоке (0 это кол-ва байт для смещения)
	long sz = ftell(file);      // двоичный файл, количество байтов
	fseek(file, 0, SEEK_SET);   // указатель file на начало файла

	if(method == HTTP_GET)//SEEK_SET Начало файла 0
	{
		data = (char *) realloc(data, sz); //изменили размер буфера
		fread(data, sz, 1, file); //поместили файл в буфер
	}
	fclose(file);

	// отправляем содержимое заголовка и файла
	send(sock, ok, strlen(ok), 0);
	printf("\n%s - is found (HTTP/1.1 200 OK)", file_path);
	sprintf(file_path, "%d\r\n", sz);
	send(sock, file_path, strlen(file_path), 0);
	send(sock, ctype, strlen(ctype), 0);
	send(sock, type, strlen(type), 0);
	send(sock, end, strlen(end), 0);

	if(method == HTTP_GET)
	{
		send(sock, data, sz, 0);
		free(data); // освобождаем память
	}
	printf("\nType : %s\nSize : %d byte\n", type, sz);
}

// поток работы с клиентом
DWORD WINAPI server(SOCKET sock)
{
	char file_name[200] = {""};
	int method = recv_data(sock, file_name);
	send_data(sock, file_name, method); //отправка информации

	closesocket(sock);
	return 0;
}

int main()
{
	WSAData wsaData; //структура сокета
	sockaddr_in addr_Sock; //адрес конечной точки
	SOCKET listen_Sock;
	WSAStartup(MAKEWORD(2, 2), &wsaData); // инициализация для использования сокетов процессом

	//создание сокета, который связан с клиентом
	if((listen_Sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET)
	{
		printf("Unable to create socket\n");
		WSACleanup();
		system("pause");
		return INVALID_SOCKET;
	}

	//настраиваем сокет
	addr_Sock.sin_family = AF_INET; // семейство протоколов AF_INET
	addr_Sock.sin_addr.s_addr = INADDR_ANY; //IP-адрес компьютера (смотрим ipconfig IPv4) 
	addr_Sock.sin_port = htons(2003); //номер порта (2000 + 3 бригада)

	// связываем сокет с данными из addr_Sock
	if(bind(listen_Sock, (LPSOCKADDR) &addr_Sock, sizeof(sockaddr)) == SOCKET_ERROR)
	{
		printf("Unable to bind\n");
		WSACleanup();
		system("pause");
		return SOCKET_ERROR;
	}

	// слушаем сокет
	if(listen(listen_Sock, 1) == SOCKET_ERROR)
	{
		printf("Unable to listen\n");
		WSACleanup();
		system("pause");
		return SOCKET_ERROR;
	}

	while(true)
		CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE) &server, (void *) accept(listen_Sock, 0, 0), NULL, NULL);
	//accept - разрешение на входящее соединение (сокет, указатель на буфер, указатель на длину структуры)

	return 0;
}


