#include <Windows.h>
#include <iostream>
#include <string>

#define BUFSIZE 1024

using namespace std;

int main(int argc, char* argv[]) {
    setlocale(LC_ALL, "Russian");

    // Проверка аргументов командной строки
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " <client number>" << endl;
        return -1;
    }

    int clientNum = atoi(argv[1]);
    if (clientNum < 0) {
        cerr << "Client number must be non-negative" << endl;
        return -1;
    }

    HANDLE hPipe;
    BOOL fSuccess;
    DWORD dwRead;
    char buffer[BUFSIZE];

    // Формирование имени канала для данного клиента
    string pipeName = "\\\\.\\pipe\\worddist_pipe_" + to_string(clientNum);

    // Подключение к именованному каналу
    while (true) {
        hPipe = CreateFileA(
            pipeName.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            0,
            NULL);

        if (hPipe != INVALID_HANDLE_VALUE) {
            break;
        }

        // Обработка ошибок подключения
        if (GetLastError() != ERROR_PIPE_BUSY) {
            cerr << "Failed to connect to pipe. GLE=" << GetLastError() << endl;
            return -1;
        }

        // Ожидание освобождения канала
        if (!WaitNamedPipeA(pipeName.c_str(), 20000)) {
            cerr << "Could not open pipe: 20 second wait timed out" << endl;
            return -1;
        }
    }

    cout << ">>> Client " << clientNum << " connected to server <<<" << endl;

    // Основной цикл работы клиента
    while (true) {
        // Чтение данных от сервера
        fSuccess = ReadFile(
            hPipe,
            buffer,
            BUFSIZE,
            &dwRead,
            NULL);

        // Обработка ошибок чтения
        if (!fSuccess || dwRead == 0) {
            if (GetLastError() == ERROR_BROKEN_PIPE) {
                cout << "Server disconnected." << endl;
            }
            else {
                cerr << "Read error. GLE=" << GetLastError() << endl;
            }
            break;
        }

        buffer[dwRead] = '\0';
        string word(buffer);

        // Проверка команды завершения работы
        if (word == "END") {
            cout << "Received termination command." << endl;
            break;
        }

        cout << "Received word: " << word << endl;

        // Отправка подтверждения серверу
        string ack = "ACK_" + to_string(clientNum);
        fSuccess = WriteFile(
            hPipe,
            ack.c_str(),
            ack.size(),
            &dwRead,
            NULL);

        if (!fSuccess) {
            cerr << "Write error. GLE=" << GetLastError() << endl;
            break;
        }
    }

    // Закрытие дескриптора канала
    CloseHandle(hPipe);
    cout << "Client " << clientNum << " shutting down." << endl;
    return 0;
}