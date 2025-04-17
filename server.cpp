#include <Windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

#define BUFSIZE 1024
#define PIPE_TIMEOUT 5000

using namespace std;

// Функция для чтения слов из файла
vector<string> readF(const string& filename) {
    vector<string> words;
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Error opening file: " << filename << endl;
        return words;
    }

    string word;
    while (file >> word) {
        words.push_back(word);
    }
    file.close();
    return words;
}

int main(int argc, char* argv[]) {

    // Проверка аргументов командной строки
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " <number of clients>" << endl;
        return 1;
    }

    int numClients = atoi(argv[1]);
    if (numClients < 3) {
        cerr << "Number of clients must be at least 3" << endl;
        return 1;
    }

    // Запрос имени файла у пользователя
    cout << "Enter filename: ";
    string filename;
    getline(cin, filename);

    // Чтение слов из файла
    vector<string> words = readF(filename);
    if (words.empty()) {
        cout << "File is empty or cannot be read." << endl;
        return 1;
    }

    cout << "Found " << words.size() << " words in the file." << endl;
    cout << "Waiting for " << numClients << " clients to connect..." << endl;

    // Создание именованных каналов для каждого клиента
    vector<HANDLE> hPipes(numClients);
    for (int i = 0; i < numClients; i++) {
        string pipeName = "\\\\.\\pipe\\worddist_pipe_" + to_string(i);
        
        hPipes[i] = CreateNamedPipeA(
            pipeName.c_str(),
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            numClients,
            BUFSIZE * 4,
            BUFSIZE * 4,
            PIPE_TIMEOUT,
            NULL);

        if (hPipes[i] == INVALID_HANDLE_VALUE) {
            cerr << "Error creating pipe for client " << i << ". GLE=" << GetLastError() << endl;
            return -1;
        }
    }

    // Ожидание подключения всех клиентов
    for (int i = 0; i < numClients; i++) {
        cout << "Waiting for client " << i << " to connect..." << endl;
        
        BOOL connected = ConnectNamedPipe(hPipes[i], NULL) ? 
            TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);

        if (!connected) {
            cerr << "Error connecting client " << i << ". GLE=" << GetLastError() << endl;
            CloseHandle(hPipes[i]);
            return -1;
        }
        cout << "Client " << i << " connected successfully" << endl;
    }

    // Распределение слов между клиентами 
    int currentClient = 0;
    for (size_t i = 0; i < words.size(); i++) {
        DWORD dwWritten;
        string word = words[i];
        
        BOOL fSuccess = WriteFile(
            hPipes[currentClient],
            word.c_str(),
            word.size(),
            &dwWritten,
            NULL);

        if (!fSuccess) {
            cerr << "Error writing to client " << currentClient << ". GLE=" << GetLastError() << endl;
            break;
        }

        cout << "Sent to client " << currentClient << ": " << word << endl;

        // Получение подтверждения от клиента
        char buffer[BUFSIZE];
        DWORD dwRead;
        fSuccess = ReadFile(
            hPipes[currentClient],
            buffer,
            BUFSIZE,
            &dwRead,
            NULL);

        if (!fSuccess || dwRead == 0) {
            cerr << "Error reading from client " << currentClient << ". GLE=" << GetLastError() << endl;
            break;
        }

        buffer[dwRead] = '\0';
        cout << "Client " << currentClient << " acknowledgment: " << buffer << endl;

        // Переход к следующему клиенту
        currentClient = (currentClient + 1) % numClients;
    }

    // Завершение работы - отправка сообщения о завершении всем клиентам
    for (int i = 0; i < numClients; i++) {
        DWORD dwWritten;
        string endMsg = "END";
        WriteFile(hPipes[i], endMsg.c_str(), endMsg.size(), &dwWritten, NULL);
        CloseHandle(hPipes[i]);
    }

    cout << "Word distribution completed successfully." << endl;
}