#include "function.h"
#include <zstd.h> 


SOCKET                             sockctr;
const char* IP = "";
const char* PORT = "";
int SERVER_PORT = std::stoi(PORT);
BOOL                               RunOnce = FALSE;
CRITICAL_SECTION                   cs;
LONG                               CHUNK_SIZE;
vector<HANDLE>                     threadHandles;
vector<atomic<bool>*>              threadRunningFlags;
atomic<int>                        desiredThreadCount(15);
atomic<bool>                       globalRunning(true);
atomic<bool>                       start_flag(false);
atomic<bool>                       Flagendbuildstruct(false);
LARGE_INTEGER                      ReceivedVolumeByTime;
DWORD                              timer = 0;
DWORD                              MehtodFlag = 1;
vector<HANDLE>                     TaskThreads;
LONG                               g_nextIndex = 0;
deque<SendFileStruct*>             sendFileQueue;
LARGE_INTEGER                      totalBytesSent;
vector<char>                       g_jsonBuffer;
queue<Task>                        taskQueue;
std::atomic<int> currentIndex(0);
std::atomic<bool> connected(false);
std::atomic<bool> stopAll(false);
CRITICAL_SECTION csPrint;
vector<int> ListTaskIds;
std::mutex queueMutex;



namespace UL_Socket {
    BOOL recvWithSize(SOCKET sock, vector<char>& buffer, int size) {
        buffer.resize(size);
        int received = 0;

        while (received < size) {
            // Use select() to wait for data with a timeout
            fd_set readSet;
            FD_ZERO(&readSet);
            FD_SET(sock, &readSet);

            timeval timeout;
            timeout.tv_sec = 15;
            timeout.tv_usec = 0;

            int sel = select(0, &readSet, NULL, NULL, &timeout);
            if (sel <= 0) {
                // select() timeout or error
                return false;
            }

            // Socket is ready to read
            int ret = recv(sock, &buffer[received], size - received, 0);
            if (ret <= 0) {
                return false; // Socket closed or error
            }

            received += ret;
        }

        return true;
    }

    bool IsSocketAlive(SOCKET sock) {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(sock, &readSet);

        timeval timeout = { 0, 0 }; // Non-blocking check

        int result = select(0, &readSet, NULL, NULL, &timeout);
        if (result == SOCKET_ERROR) return false;

        if (FD_ISSET(sock, &readSet)) {
            char tmp;
            int ret = recv(sock, &tmp, 1, MSG_PEEK);
            if (ret == 0 || ret == SOCKET_ERROR) {
                return false; // Socket closed or error
            }
        }

        return true; // Socket appears alive
    }

    SOCKET ConnectToServer(const char* serverAddress, const char* port) {
        WSADATA wsaData;
        SOCKET connectSocket = INVALID_SOCKET;
        struct addrinfo* result = nullptr, * ptr = nullptr, hints;

        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            cerr << "WSAStartup failed\n";
            return INVALID_SOCKET;
        }

        ZeroMemory(&hints, sizeof(hints));
        hints.ai_family = AF_INET;        // IPv4
        hints.ai_socktype = SOCK_STREAM;  // TCP
        hints.ai_protocol = IPPROTO_TCP;

        if (getaddrinfo(serverAddress, port, &hints, &result) != 0) {
            cerr << "getaddrinfo failed\n";
            WSACleanup();
            return INVALID_SOCKET;
        }

        for (ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
            connectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
            if (connectSocket == INVALID_SOCKET) {
                cerr << "Error at socket(): " << WSAGetLastError() << "\n";
                WSACleanup();
                return INVALID_SOCKET;
            }

            if (connect(connectSocket, ptr->ai_addr, (int)ptr->ai_addrlen) == SOCKET_ERROR) {
                closesocket(connectSocket);
                connectSocket = INVALID_SOCKET;
                continue;
            }

            break; // Successfully connected
        }

        freeaddrinfo(result);

        if (connectSocket == INVALID_SOCKET) {
            //cerr << "Unable to connect to server!\n";
            WSACleanup();
            return INVALID_SOCKET;
        }

        return connectSocket; // Caller is responsible for closing socket and cleaning up Winsock
    }

    BOOL SocketInitialization(SOCKET& sock, int Flag) {
        int flag = 1111;
        int SentFlag1111 = send(sock, (char*)&flag, sizeof(int), 0); // flag 1111
        if (SentFlag1111 == SOCKET_ERROR) {
            closesocket(sock);
            WSACleanup();
            return FALSE;
        }
        DWORD serialNumber = 0;
        GetVolumeInformationA("C:\\", 0, 0, &serialNumber, 0, 0, 0, 0);

        int intSerialNumber = static_cast<int>(serialNumber);

        // Create a buffer and append values
        vector<char> buffer;
        buffer.insert(buffer.end(), reinterpret_cast<char*>(&Flag), reinterpret_cast<char*>(&Flag) + sizeof(int));
        buffer.insert(buffer.end(), reinterpret_cast<char*>(&intSerialNumber), reinterpret_cast<char*>(&intSerialNumber) + sizeof(int));


        // Send the buffer
        int SentFlagAndId = send(sock, buffer.data(), buffer.size(), 0); // static id
        if (SentFlagAndId == SOCKET_ERROR) {
            closesocket(sock);
            WSACleanup();
            return FALSE;
        }
        return TRUE;
    }
}

namespace UL_SocketTools {
    VOID TaskProcessor(void* Task_Json) {
        std::string* task = static_cast<std::string*>(Task_Json);
        json item = json::parse(*task);
        //json result = UL_JsonTools::parseBufferToJson(task);

        int task_flag = item["task_flag"];
        int task_id = item["task_id"];

        switch (task_flag) {
        case 1: // Load Drives and Share Folders
        {
            ThreadParams* params = new ThreadParams{ task_flag, task_id,{} };
            HANDLE hThread = CreateThread(
                nullptr,
                0,
                UL_SwitchCase::Case1_LoadDriveAndShare,
                params,
                0,
                nullptr
            );
            if (hThread) {
                TaskThreads.push_back(hThread);
            }
            break;
        }
        case 2: // Load Complete
        {
            vector<wstring> vector_pathToload;
            for (const auto& path : item["PATH"]) {
                //string utf8Path = path.get<string>();
                wstring widePath = UL_win32Tools::utf8_to_wstring(path.get<string>());
                vector_pathToload.push_back(widePath);
            }

            ThreadParams* params = new ThreadParams{ task_flag, task_id,vector_pathToload };
            HANDLE hThread = CreateThread(
                nullptr,
                0,
                UL_SwitchCase::Case2_LoadComplete,
                params,
                0,
                nullptr
            );
            if (hThread) {
                TaskThreads.push_back(hThread);
            }
            WaitForSingleObject(hThread, INFINITE);
            break;
        }
        case 3: // Load layer
        {
            vector<wstring> vector_pathToload;
            for (auto& path : item["PATH"]) {
                string utf8Path = path.get<string>();
                wstring widePath = UL_win32Tools::utf8_to_wstring(utf8Path);
                vector_pathToload.push_back(widePath);
            }
            ThreadParams* params = new ThreadParams{ task_flag, task_id,vector_pathToload };
            HANDLE hThread = CreateThread(
                nullptr,
                0,
                UL_SwitchCase::Case3_LoadLayer,
                params,
                0,
                nullptr
            );
            if (hThread) {
                TaskThreads.push_back(hThread);
            }
            WaitForSingleObject(hThread, INFINITE);

            break;
        }
        case 4: // Upload
        {
            vector<wstring> vector_pathToload;
            if (item.contains("PATH") && item["PATH"].is_array()) {
                for (const auto& path : item["PATH"]) {
                    string utf8Path = path.get<string>();
                    wstring widePath = UL_win32Tools::utf8_to_wstring(utf8Path);
                    vector_pathToload.push_back(widePath);
                }
            }
            string vector_pathes = "";
            if (item.contains("pathes")) {
                auto& path = item["pathes"];
                string utf8Path = path.get<string>();
                wstring widePath = UL_win32Tools::utf8_to_wstring(utf8Path);
                vector_pathes = UL_win32Tools::wstring_to_utf8(widePath);
            }
            bool resFlag = false;
            if (item.contains("is_resume") && item["is_resume"].is_boolean()) {
                resFlag = item["is_resume"].get<bool>();
            }
            vector<wstring> vector_Excludes;
            if (item.contains("Excludes") && item["Excludes"].is_array()) {
                for (const auto& path : item["Excludes"]) {
                    string utf8Path = path.get<string>();
                    wstring widePath = UL_win32Tools::utf8_to_wstring(utf8Path);
                    vector_Excludes.push_back(widePath);
                }
            }
            vector<wstring> vector_Extensions;
            if (item.contains("Extentions") && item["Extentions"].is_array()) {
                for (const auto& path : item["Extentions"]) {
                    string utf8Path = path.get<string>();
                    wstring widePath = UL_win32Tools::utf8_to_wstring(utf8Path);
                    vector_Extensions.push_back(widePath);
                }
            }
            ThreadParamsForCase4* params = new ThreadParamsForCase4{ task_flag, task_id,vector_pathToload,vector_pathes,resFlag,vector_Excludes,vector_Extensions };
            HANDLE hThread = CreateThread(
                nullptr,
                0,
                UL_SwitchCase::Case4_UpLoad,
                params,
                0,
                nullptr
            );
            if (hThread) {
                TaskThreads.push_back(hThread);
            }
            WaitForSingleObject(hThread, INFINITE);
            break;
        }
        case 5: {
            wstring path = L"";
            int intPart;
            if (item.contains("error12")) {
                auto p = item["error12"]["path"];
                auto part = item["error12"]["part"];
                intPart = part.get<int>();
                string utf8Path = p.get<string>();
                wstring path = UL_win32Tools::utf8_to_wstring(utf8Path);
            }
            for (auto it = sendFileQueue.begin(); it != sendFileQueue.end(); ++it) {
                if ((*it)->filePath == path) {
                    delete* it;
                    sendFileQueue.erase(it);
                    std::wcout << L"Element removed." << std::endl;
                    break;
                }
            }

        }
        default:

            break;
        }
        delete task;
    }

    DWORD WINAPI TaskManager(LPVOID lpParam) {
        while (TRUE) {
            if (!taskQueue.empty()) {
                Task task = taskQueue.front();

                std::future<void> fut = std::async(std::launch::async, task.func, task.json_item);
                fut.get(); // Wait for current task to finish before continuing
                taskQueue.pop();
            }
            else
            {
                Sleep(5000);
            }
        }
    }

    BOOL SettingMainVariables(SOCKET& sock) {
        vector<char> recv16Byte;
        if (!UL_Socket::recvWithSize(sock, recv16Byte, 16)) {
            closesocket(sock);
            WSACleanup();
            return FALSE;
        }
        int size = (extractAndRemoveFlag(recv16Byte, 4));
        CHUNK_SIZE = static_cast<long>(size * 1024 * 1024);
        cout << "chunk " << CHUNK_SIZE << endl;
        desiredThreadCount = (extractAndRemoveFlag(recv16Byte, 4));
        cout << "thraed " << desiredThreadCount << endl;
        ReceivedVolumeByTime.QuadPart = (extractAndRemoveFlag(recv16Byte, 4));
        cout << "volume " << ReceivedVolumeByTime.QuadPart << endl;
        timer = (extractAndRemoveFlag(recv16Byte, 4));
        cout << "timetosleep " << timer << endl;

        return TRUE;
    }

    INT extractAndRemoveFlag(vector<char>& buffer, size_t sizeofByte) {
        if (buffer.size() < sizeofByte) {
            throw runtime_error("Buffer too small to extract flag");
        }

        int number = 0;
        for (size_t i = 0; i < sizeofByte; ++i) {
            number |= static_cast<int>(static_cast<uint8_t>(buffer[i])) << (i * 8);  // LSB first
        }

        buffer.erase(buffer.begin(), buffer.begin() + sizeofByte);
        return number;
    }

    vector<pair<bool, DWORD>> CheckThreadsStatus(vector<HANDLE>& threads) {
        vector<pair<bool, DWORD>> result;
        result.reserve(threads.size());

        for (auto it = threads.begin(); it != threads.end(); ) {
            DWORD wait = WaitForSingleObject(*it, 0);
            if (wait == WAIT_OBJECT_0) {
                // Thread has exited; retrieve its exit code
                DWORD exitCode = 0;
                if (GetExitCodeThread(*it, &exitCode)) {
                    result.emplace_back(true, exitCode);
                }
                else {
                    result.emplace_back(true, STILL_ACTIVE);
                }

                CloseHandle(*it);           // Important: release OS resources
                it = threads.erase(it);     // Remove from vector, continue loop
            }
            else {
                result.emplace_back(false, 0);
                ++it;
            }
        }

        return result;
    }

    DWORD WINAPI Trap(LPVOID lpParam) {
        while (TRUE) {
            //while (!start_flag.load(memory_order_acquire)) {
            //    Sleep(1000);
            //}
            if (!UL_Socket::IsSocketAlive(sockctr)) {
                cout << "[TRAP] sockctr is Off" << endl;
                return 0;
            }
            //if (sockctr == INVALID_SOCKET) {
            //    cout << "[TRAP] sockctr is Off" << endl;
            //    Sleep(1000);
            //    return 0;
            //}
            Sleep(10000);


            auto statuses = CheckThreadsStatus(TaskThreads);
            for (size_t i = 0; i < statuses.size(); ++i) {
                if (statuses[i].first) {
                    string numStr = to_string(statuses[i].second);
                    vector<int> digits;
                    for (char ch : numStr) {
                        digits.push_back(ch - '0'); // Convert char to int
                    }
                    printf("[TASK] Thread has exited with code %lu and Task_flag %lu\n", digits[1], digits[0]);
                    //TaskThreads.erase(TaskThreads.begin() + i);
                }
                else {
                    printf("[TASK] Thread %zu is still running\n");
                }
            }

            SOCKET sock;

            sock = UL_Socket::ConnectToServer(IP, PORT);
            if (sock == INVALID_SOCKET) {
                closesocket(sock);
                cout << "[TRAP]Faild to create socket TRAP send" << endl;
                continue;
            }
            int result;
            result = UL_Socket::SocketInitialization(sock, 4456);
            //int result;
            //do
            //{
            //    result = UL_Socket::SocketInitialization(sock, 4456);
            //} while (result != TRUE);
            //cout << "[TRAP] send" << endl;

        }
    }

    BOOL SendSizeFlagAndBuffer(SOCKET sock, int TaskId, const vector<char>& buffer) {
        int totalSize = static_cast<int>(buffer.size());

        char flagBytes[4];
        memcpy(flagBytes, &TaskId, sizeof(flagBytes));

        char sizeofcontent[4];
        memcpy(sizeofcontent, &totalSize, sizeof(sizeofcontent));



        // Create WSABUF array: [flag][size][buffer]
        WSABUF wsabufs[3];


        wsabufs[0].buf = flagBytes;
        wsabufs[0].len = sizeof(flagBytes);

        wsabufs[1].buf = sizeofcontent;
        wsabufs[1].len = sizeof(sizeofcontent);

        wsabufs[2].buf = const_cast<char*>(buffer.data());
        wsabufs[2].len = static_cast<ULONG>(buffer.size());

        DWORD bytesSent = 0;
        int result = WSASend(sock, wsabufs, 3, &bytesSent, 0, nullptr, nullptr);
        if (result == SOCKET_ERROR) {
            cerr << "WSASend failed with error: " << WSAGetLastError() << endl;
            return false;
        }

        return true;
    }

    BOOL SendSizeFlagAndBuffer_withoutSizeFlag(SOCKET sock, int TaskId, int bufferSize, BYTE* buffer) {


        char flagBytes[4];
        memcpy(flagBytes, &TaskId, sizeof(flagBytes));




        // Create WSABUF array: [flag][size][buffer]
        WSABUF wsabufs[2];


        wsabufs[0].buf = flagBytes;
        wsabufs[0].len = sizeof(flagBytes);

        wsabufs[1].buf = reinterpret_cast<char*>(buffer);
        wsabufs[1].len = static_cast<ULONG>(bufferSize);

        DWORD bytesSent = 0;
        int result = WSASend(sock, wsabufs, 2, &bytesSent, 0, nullptr, nullptr);
        if (result == SOCKET_ERROR) {
            cerr << "WSASend failed with error: " << WSAGetLastError() << endl;
            return false;
        }

        return true;
    }
}

namespace UL_win32Tools {
    wstring utf8_to_wstring(const string& str) {
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
        if (size_needed <= 0) return L"";
        wstring wstrTo(size_needed - 1, 0); // exclude null terminator
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstrTo[0], size_needed);
        return wstrTo;
    }

    string wstring_to_utf8(const wstring& wstr) {
        if (wstr.empty()) return {};

        int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
        string strTo(size_needed - 1, 0); // exclude null terminator
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &strTo[0], size_needed, nullptr, nullptr);
        return strTo;
    }

    void add_logical_drives(json& resultJson) {
        wchar_t driveStrings[256];
        DWORD size = GetLogicalDriveStringsW(sizeof(driveStrings) / sizeof(driveStrings[0]), driveStrings);

        for (wchar_t* drive = driveStrings; *drive; drive += wcslen(drive) + 1) {
            if (GetDriveTypeW(drive) == DRIVE_NO_ROOT_DIR)
                continue;
            if (GetVolumeInformationW(drive, nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0)) {
                string drive_utf8 = wstring_to_utf8(drive);

                if (!drive_utf8.empty() && drive_utf8.back() == L'\\') {
                    drive_utf8.pop_back();
                }

                json entry;
                entry["PATH"] = drive_utf8;
                entry["files"] = json::array();
                entry["folders"] = json::array();
                entry["is_main_layer"] = true;

                resultJson.push_back(entry);
            }
        }
    }

    void add_shared_folders(json& resultJson) {
        PSHARE_INFO_1 pBuf = nullptr;
        DWORD entriesRead = 0, totalEntries = 0, resumeHandle = 0;
        NET_API_STATUS nStatus;

        do {
            nStatus = NetShareEnum(
                nullptr,                 // local computer
                1,                       // level
                (LPBYTE*)&pBuf,
                MAX_PREFERRED_LENGTH,
                &entriesRead,
                &totalEntries,
                &resumeHandle
            );

            if ((nStatus == NERR_Success || nStatus == ERROR_MORE_DATA) && pBuf != nullptr) {
                for (DWORD i = 0; i < entriesRead; i++) {
                    wstring netPath = L"\\\\" + wstring(L"127.0.0.1") + L"\\" + pBuf[i].shi1_netname;
                    string share_utf8 = wstring_to_utf8(netPath);

                    json entry;
                    entry["PATH"] = share_utf8;
                    entry["files"] = json::array();
                    entry["folders"] = json::array();
                    entry["is_main_layer"] = true;

                    resultJson.push_back(entry);
                }
            }

            if (pBuf != nullptr) {
                NetApiBufferFree(pBuf);
                pBuf = nullptr;
            }
        } while (nStatus == ERROR_MORE_DATA);
    }

    vector<char> getDrivesAndSharedFolders() {
        json resultJson = json::array();

        add_logical_drives(resultJson);
        add_shared_folders(resultJson);

        string jsonStr = resultJson.dump();
        return vector<char>(jsonStr.begin(), jsonStr.end());
    }

    void TraverseDirectory(const vector<wstring>& folders, vector<char>& buffer, BOOL OneStep) {
        for (const auto& folderPath : folders) {
            // Build this directory’s JSON
            json thisDir;
            thisDir["PATH"] = wstring_to_utf8(folderPath);
            thisDir["files"] = json::array();
            thisDir["folders"] = json::array();

            // Collect subdirectories for recursion
            vector<wstring> subDirs;

            // Enumerate
            WIN32_FIND_DATA findData;
            HANDLE hFind = FindFirstFile((folderPath + L"\\*").c_str(), &findData);
            if (hFind != INVALID_HANDLE_VALUE) {
                do {
                    wstring name = findData.cFileName;
                    if (name == L"." || name == L"..") continue;

                    bool isDir = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                    wstring fullPath = folderPath + L"\\" + name;

                    if (isDir) {
                        subDirs.push_back(fullPath);
                        string fileName = wstring_to_utf8(name);
                        json fileEntry = {
                            {"folder_name", fileName},
                            {"Modified", FileTimeToString(findData.ftLastWriteTime)}
                        };
                        thisDir["folders"].push_back(fileEntry);
                        if (OneStep) {

                        }
                    }
                    else {
                        ULARGE_INTEGER size;
                        size.LowPart = findData.nFileSizeLow;
                        size.HighPart = findData.nFileSizeHigh;

                        thisDir["files"].push_back({
                            {"File_name",     wstring_to_utf8(name)},
                            {"File_size",     size.QuadPart},
                            {"Last_Modified", FileTimeToString(findData.ftLastWriteTime)}
                            });
                    }
                } while (FindNextFile(hFind, &findData));
                FindClose(hFind);

                // Recurse into subdirectories if not OneStep
                if (!OneStep && !subDirs.empty()) {
                    TraverseDirectory(subDirs, buffer, OneStep);
                }

                // Dump JSON and append to buffer (with comma if needed)
                string block = thisDir.dump();
                if (buffer.size() > 1) {
                    // previous content not just the initial '[' ? add comma separator
                    buffer.push_back(',');
                }
                buffer.insert(buffer.end(), block.begin(), block.end());
            }


        }
    }

    string FileTimeToString(FILETIME ft) {
        SYSTEMTIME stUTC, stLocal;
        FileTimeToSystemTime(&ft, &stUTC);
        SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);

        ostringstream oss;
        oss << setfill('0')
            << setw(4) << stLocal.wYear << "-"
            << setw(2) << stLocal.wMonth << "-"
            << setw(2) << stLocal.wDay << " "
            << setw(2) << stLocal.wHour << ":"
            << setw(2) << stLocal.wMinute << ":"
            << setw(2) << stLocal.wSecond;
        return oss.str();
    }

}

namespace UL_SwitchCase {
    DWORD WINAPI Case1_LoadDriveAndShare(LPVOID lpParam) {
        ThreadParams* params = reinterpret_cast<ThreadParams*>(lpParam);
        int Task_flag = params->Task_flag;


        vector<char> Buffer_drive_share = UL_win32Tools::getDrivesAndSharedFolders(); // get drive and share

        SOCKET sock;
        do
        {
            sock = UL_Socket::ConnectToServer(IP, PORT);
        } while (sock == INVALID_SOCKET);

        int result;
        do
        {
            result = UL_Socket::SocketInitialization(sock, 7921);
        } while (result != TRUE);

        if (!UL_SocketTools::SendSizeFlagAndBuffer(sock, params->Task_id, Buffer_drive_share)) {
            cout << "[TASK FLAG 1][Failed] to Send Task 1 (load drives and share)" << endl;
            delete params;
            closesocket(sock);
            return static_cast<DWORD>(Task_flag * 10 + 1);
        }
        cout << "[TASK FLAG 1] Send Task 1 (load drives and share)" << endl;
        delete params;
        closesocket(sock);
        return static_cast<DWORD>(Task_flag * 10 + 0);
    }

    DWORD WINAPI Case2_LoadComplete(LPVOID lpParam) {
        ThreadParams* params = reinterpret_cast<ThreadParams*>(lpParam);
        int Task_flag = params->Task_flag;
        vector<char> Buffer_loadComp;

        Buffer_loadComp.push_back('[');
        UL_win32Tools::TraverseDirectory(params->vecWstr, Buffer_loadComp, FALSE);
        Buffer_loadComp.push_back(']');

        SOCKET sock;
        do
        {
            sock = UL_Socket::ConnectToServer(IP, PORT);
        } while (sock == INVALID_SOCKET);

        int result;
        do
        {
            result = UL_Socket::SocketInitialization(sock, 7921);
        } while (result != TRUE);

        if (!UL_SocketTools::SendSizeFlagAndBuffer(sock, params->Task_id, Buffer_loadComp)) {
            cout << "[TASK FLAG 2][Failed] to Send Task 2 (Load complete)" << endl;
            delete params;
            closesocket(sock);
            return static_cast<DWORD>(Task_flag * 10 + 1);
        }
        cout << "[TASK FLAG 2] Send Task 2 (Load complete)" << endl;
        delete params;
        closesocket(sock);
        return static_cast<DWORD>(Task_flag * 10 + 0);
    }

    DWORD WINAPI Case3_LoadLayer(LPVOID lpParam) {
        ThreadParams* params = reinterpret_cast<ThreadParams*>(lpParam);
        int Task_flag = params->Task_flag;
        vector<char> Buffer_loadComp;

        Buffer_loadComp.push_back('[');
        UL_win32Tools::TraverseDirectory(params->vecWstr, Buffer_loadComp, TRUE);
        Buffer_loadComp.push_back(']');

        SOCKET sock;
        do
        {
            sock = UL_Socket::ConnectToServer(IP, PORT);
        } while (sock == INVALID_SOCKET);

        int result;
        do
        {
            result = UL_Socket::SocketInitialization(sock, 7921);
        } while (result != TRUE);

        if (!UL_SocketTools::SendSizeFlagAndBuffer(sock, params->Task_id, Buffer_loadComp)) {
            cout << "[TASK FLAG 3][Failed] to Send Task 3 (Load Layer)" << endl;
            delete params;
            closesocket(sock);
            return static_cast<DWORD>(Task_flag * 10 + 1);
        }
        cout << "[TASK FLAG 3] Send Task 3 (Load Layer)" << endl;
        delete params;
        closesocket(sock);
        return static_cast<DWORD>(Task_flag * 10 + 0);
    }

    DWORD WINAPI Case4_UpLoad(LPVOID lpParam) {
        ThreadParamsForCase4* params = reinterpret_cast<ThreadParamsForCase4*>(lpParam);
        int Task_flag = params->Task_flag;
        InitializeCriticalSection(&csPrint);
        if (params->resumeFlag) {
            // resume
            if (!(std::find(ListTaskIds.begin(), ListTaskIds.end(), params->Task_id) == ListTaskIds.end())) {
                std::cout << "[TASK FLAG 4] Task ID already in list .\n";
                delete params;
                return static_cast<DWORD>(Task_flag * 10 + 0);
            }
            ListTaskIds.push_back(params->Task_id);
            UL_JsonTools::split_and_parse_json_objects(params->vector_pathes);
            if (!sendFileQueue.empty()) {
                //UL_Sender::MainSenderThread(params->Task_id, params->vecWstr, 1); // 1 = resume
                std::cout << "[TASK FLAG 4] Starting  Resume...\n";

                //connected.store(false);

                //std::thread monitor(UL_Sender::monitorThread);

                std::vector<std::thread> threads;
                for (int i = 0; i < desiredThreadCount; ++i)
                    threads.emplace_back(UL_Sender::workerThread, i, params->Task_id);

                //monitor.join();
                for (auto& t : threads) t.join();
            }

            // 4935 task end
            UL_Sender::SendFlagUploadEnd(params->Task_id);

            cout << "[TASK FLAG 4] Send Task 4 (resume)" << endl;

            //connected.store(false);
            stopAll.store(false);
            delete params;
            return static_cast<DWORD>(Task_flag * 10 + 0);
        }
        else
        {
            ListTaskIds.push_back(params->Task_id);
            vector<char> Buffer_loadComp;

            Buffer_loadComp.push_back('[');
            UL_Sender::collectJsonFolderBlobs(params->vecWstr, params->vector_Extensions, params->vector_Excludes, Buffer_loadComp);
            Buffer_loadComp.push_back(']');



            SOCKET sock;
            do
            {
                sock = UL_Socket::ConnectToServer(IP, PORT);
            } while (sock == INVALID_SOCKET);

            int result;
            do
            {
                result = UL_Socket::SocketInitialization(sock, 3248);
            } while (result != TRUE);
            if (!UL_SocketTools::SendSizeFlagAndBuffer(sock, params->Task_id, Buffer_loadComp)) {
                cout << "[TASK FLAG 4][Failed] to Send FLOW. (upload)" << endl;
                closesocket(sock);
                delete params;
                return static_cast<DWORD>(Task_flag * 10 + 1);
            }
            cout << "[TASK FLAG 4] Send FLOW. (upload)" << endl;
            closesocket(sock);

            UL_Sender::verticalTraversal(params->vecWstr, params->vector_Extensions, params->vector_Excludes);
            if (!sendFileQueue.empty()) {
                std::cout << "[TASK FLAG 4] Starting uploader...\n";

                std::vector<std::thread> threads;
                if (sendFileQueue.size() <= desiredThreadCount) {
                    int size = sendFileQueue.size();
                    for (int i = 0; i < size; ++i)
                        threads.emplace_back(UL_Sender::workerThread, i, params->Task_id);
                }
                else
                {
                    for (int i = 0; i < desiredThreadCount; ++i)
                        threads.emplace_back(UL_Sender::workerThread, i, params->Task_id);
                }


                //monitor.join();
                for (auto& t : threads) t.join();
            }
            


            UL_Sender::SendFlagUploadEnd(params->Task_id);
            cout << "[SUCCESS] Send Task 4 (Upload)" << endl;



            //connected.store(false);
            stopAll.store(false);
            delete params;
            return static_cast<DWORD>(Task_flag * 10 + 0);
        }
        DeleteCriticalSection(&csPrint);
    }
}

namespace UL_JsonTools {
    json parseBufferToJson(vector<char>* buffer) {
        if (!buffer || buffer->empty()) {
            cerr << "Buffer is null or empty." << endl;
            return json();
        }
        string jsonStr(buffer->begin(), buffer->end());
        try {
            return json::parse(jsonStr);
        }
        catch (const json::parse_error& e) {
            cerr << "JSON parse error: " << e.what() << endl;
            return json();
        }
    }

    VOID split_and_parse_json_objects(const string& input) {
        size_t i = 0;
        int brace_depth = 0;
        bool in_string = false;
        size_t start = 0;
        bool capturing = false;

        while (i < input.size()) {
            char c = input[i];

            if (c == '"' && (i == 0 || input[i - 1] != '\\')) {
                in_string = !in_string;
            }

            if (!in_string) {
                if (c == '{') {
                    if (brace_depth == 0) {
                        start = i;
                        capturing = true;
                    }
                    brace_depth++;
                }
                else if (c == '}') {
                    brace_depth--;
                    if (brace_depth == 0 && capturing) {
                        std::string obj_str = input.substr(start, i - start + 1);
                        try {
                            json obj = json::parse(obj_str);

                            string PATH = obj["PATH"];
                            wstring wstrPATH = UL_win32Tools::utf8_to_wstring(PATH);

                            if (obj.contains("files") && obj["files"].is_array()) {
                                for (const auto& file : obj["files"]) {
                                    string file_name = file["file_name"];
                                    wstring wstrfile_name = UL_win32Tools::utf8_to_wstring(file_name);
                                    int total_part = file["total_part"];
                                    //int total_part = std::stoi(strtotal_part);  // value = 123

                                    vector<int> parts;
                                    if (file.contains("parts") && file["parts"].is_array()) {
                                        for (const auto& val : file["parts"]) {
                                            parts.push_back(val.get<int>());
                                        }
                                    }
                                    UL_Sender::resumeBuildStruct(wstrPATH, wstrfile_name, total_part, parts);

                                }
                            }
                        }
                        catch (const json::parse_error& e) {
                            std::cerr << "Parse error at object: " << e.what() << std::endl;
                        }
                        capturing = false;
                    }
                }
            }

            ++i;
        }

    }
}

namespace UL_Sender {
    VOID resumeBuildStruct(wstring& PATH, wstring& file_name, int& total_part, vector<int> parts) {
        wstring wfileName = PATH + L"\\" + file_name;

        WIN32_FIND_DATA findFileData;
        HANDLE hFind = FindFirstFileW(wfileName.c_str(), &findFileData);

        if (hFind == INVALID_HANDLE_VALUE) {
            DWORD error_code = GetLastError();
            std::wcout << L"-Error (1-1) " << error_code << " " << wfileName << "!" << std::endl;
            //return 0;
        }

        LARGE_INTEGER fileSize;
        fileSize.LowPart = findFileData.nFileSizeLow;
        fileSize.HighPart = findFileData.nFileSizeHigh;


        DWORD attributes = GetFileAttributes(wfileName.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES) {
            DWORD error_code = GetLastError();
            std::wcout << L"-Error (2) " << error_code << " " << wfileName << "!" << std::endl;
            FindClose(hFind);
            //return 0;
        }
        int size_PATH = WideCharToMultiByte(CP_UTF8, 0, &wfileName[0], (int)wfileName.size(), NULL, 0, NULL, NULL);
        if ((attributes & FILE_ATTRIBUTE_READONLY) == 0) {

            long long SizeFile = fileSize.QuadPart;
            int Totalpart = total_part;

            int count = 0;
            int seek = 0;

            if (fileSize.QuadPart == 0) { // sizechunk
                SendFileStruct* SendFileObject = new SendFileStruct;
                SendFileObject->totalpart = 0; // totalpart
                SendFileObject->part = 0; // part

                SendFileObject->sizechunk = 0;
                SendFileObject->sizepath = size_PATH; // sizepath
                SendFileObject->filePath = wfileName; // filePath

                sendFileQueue.push_back(SendFileObject); // append to Send queue
            }

            for (int part : parts) {


                SendFileStruct* SendFileObject = new SendFileStruct;
                SendFileObject->totalpart = Totalpart; // totalpart
                SendFileObject->part = part; // part

                if (count == Totalpart - 1) { // sizechunk
                    SendFileObject->sizechunk = SizeFile - seek;
                }
                else {
                    SendFileObject->sizechunk = CHUNK_SIZE;
                }
                SendFileObject->sizepath = size_PATH; // sizepath
                SendFileObject->filePath = wfileName; // filePath

                seek += SendFileObject->sizechunk;
                count++;

                sendFileQueue.push_back(SendFileObject); // append to Send queue

            }
        }
        else {
            DWORD error_code = GetLastError();
            std::wcerr << L"-Error (3)  " << error_code << " " << wfileName << std::endl;
        }
        FindClose(hFind);
    }

    VOID SendFlagUploadEnd(int TaskId) {
        SOCKET sock;
        do
        {
            sock = UL_Socket::ConnectToServer(IP, PORT);
        } while (sock == INVALID_SOCKET);

        int result;
        do
        {
            result = UL_Socket::SocketInitialization(sock, 4935);
        } while (result != TRUE);

        char flagBytes[4];
        memcpy(flagBytes, &TaskId, sizeof(flagBytes));
        WSABUF wsabufs[1];


        wsabufs[0].buf = flagBytes;
        wsabufs[0].len = sizeof(flagBytes);
        DWORD bytesSent = 0;
        int result1 = WSASend(sock, wsabufs, 1, &bytesSent, 0, nullptr, nullptr);
        if (result1 == SOCKET_ERROR) {
            cerr << "WSASend failed with error: " << WSAGetLastError() << endl;
        }


    }

    std::wstring NormalizePath(const std::wstring& path) {
        if (!path.empty() && path.back() == L'\\')
            return path.substr(0, path.length() - 1);
        return path;
    }

    bool StartsWithIgnoreCase(const std::wstring& fullPath, const std::wstring& prefix) {
        if (fullPath.size() < prefix.size()) return false;
        for (size_t i = 0; i < prefix.size(); ++i) {
            if (towlower(fullPath[i]) != towlower(prefix[i])) return false;
        }
        return true;
    }

    string WStringToUtf8(const wstring& wstr) {
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
        string result(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), &result[0], size_needed, nullptr, nullptr);
        return result;
    }

    wstring GetRelativePath(const wstring& full, const wstring& root) {
        if (full.find(root) == 0) {
            return full.substr(root.length() + 1); // skip backslash
        }
        return full;
    }

    //

    bool startsWith(const std::wstring& fullPath, const std::wstring& prefix) {
        return fullPath.compare(0, prefix.size(), prefix) == 0;
    }

    bool isExcluded(const std::wstring& path, const std::vector<std::wstring>& excludedDirs) {
        if (excludedDirs.empty()) return false;  // No dirs to exclude

        for (const auto& ex : excludedDirs) {
            if (startsWith(path, ex)) return true;
        }
        return false;
    }

    bool hasExtension(const std::wstring& filename, const std::vector<std::wstring>& extensions) {
        if (extensions.empty()) return true;  // Accept all extensions

        for (const auto& ext : extensions) {
            if (filename.size() >= ext.size() &&
                filename.substr(filename.size() - ext.size()) == ext)
            {
                return true;
            }
        }
        return false;
    }

    std::string fileTimeToISO(const FILETIME& ft) {
        SYSTEMTIME stUTC;
        FileTimeToSystemTime(&ft, &stUTC);

        char buffer[64];
        snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02d",
            stUTC.wYear, stUTC.wMonth, stUTC.wDay,
            stUTC.wHour, stUTC.wMinute, stUTC.wSecond);
        return std::string(buffer);
    }

    void findFilesGroupedByFolderToBuffer(const std::wstring& folderPath,
        const std::vector<std::wstring>& extensions,
        const std::vector<std::wstring>& excludes,
        std::vector<char>& buffer)
    {
        if (isExcluded(folderPath, excludes)) return;

        WIN32_FIND_DATAW findData;
        std::wstring searchPattern = folderPath + L"\\*";
        HANDLE hFind = FindFirstFileW(searchPattern.c_str(), &findData);

        if (hFind == INVALID_HANDLE_VALUE) return;

        json thisDir;
        thisDir["PATH"] = WStringToUtf8(folderPath);
        thisDir["files"] = json::array();

        do {
            const std::wstring name = findData.cFileName;

            if (name == L"." || name == L"..") continue;

            std::wstring fullPath = folderPath + L"\\" + name;

            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                findFilesGroupedByFolderToBuffer(fullPath, extensions, excludes, buffer);
            }
            else if (hasExtension(fullPath, extensions) && !isExcluded(fullPath, excludes)) {
                json fileEntry;
                fileEntry["file_name"] = WStringToUtf8(name);

                size_t dotPos = name.rfind(L'.');
                fileEntry["is_combined?"] = false;

                ULARGE_INTEGER fileSize;
                fileSize.LowPart = findData.nFileSizeLow;
                fileSize.HighPart = findData.nFileSizeHigh;

                int CHUNK_SIZE = 1024 * 1024;

                long long SizeFile = fileSize.QuadPart;
                int Totalpart = SizeFile / CHUNK_SIZE;

                if (SizeFile != 0) {
                    if (SizeFile % CHUNK_SIZE != 0) {
                        Totalpart += 1;
                    }
                    fileEntry["total_part"] = Totalpart;

                    fileEntry["parts"] = json::array();

                    thisDir["files"].push_back(fileEntry);
                }
            }
        } while (FindNextFileW(hFind, &findData) != 0);

        FindClose(hFind);

        if (!thisDir["files"].empty()) {
            string block = thisDir.dump();
            if (buffer.size() > 1) {
                buffer.push_back(',');
            }
            buffer.insert(buffer.end(), block.begin(), block.end());
        }
    }

    void collectJsonFolderBlobs(const std::vector<std::wstring>& roots,
        const std::vector<std::wstring>& extensions,
        const std::vector<std::wstring>& excludes,
        std::vector<char>& outputBuffer)
    {
        for (const auto& root : roots) {
            findFilesGroupedByFolderToBuffer(root, extensions, excludes, outputBuffer);
        }
    }


    //
    void Outline(const vector<wstring>& folders, vector<char>& buffer, BOOL OneStep) {
        for (const auto& folderPath : folders) {
            // Build this directory’s JSON
            json thisDir;
            thisDir["PATH"] = WStringToUtf8(folderPath);
            thisDir["files"] = json::array();

            // Collect subdirectories for recursion
            vector<wstring> subDirs;

            // Enumerate
            WIN32_FIND_DATA findData;
            HANDLE hFind = FindFirstFile((folderPath + L"\\*").c_str(), &findData);
            if (hFind != INVALID_HANDLE_VALUE) {
                do {
                    wstring name = findData.cFileName;
                    if (name == L"." || name == L"..") continue;

                    bool isDir = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                    wstring fullPath = folderPath + L"\\" + name;

                    if (isDir) {
                        subDirs.push_back(fullPath);

                    }
                    else {
                        ULARGE_INTEGER fileSize;
                        fileSize.LowPart = findData.nFileSizeLow;
                        fileSize.HighPart = findData.nFileSizeHigh;

                        int CHUNK_SIZE = 1024 * 1024;

                        long long SizeFile = fileSize.QuadPart;
                        int Totalpart = SizeFile / CHUNK_SIZE;

                        if (SizeFile != 0) {
                            if (SizeFile % CHUNK_SIZE != 0) {
                                Totalpart += 1;
                            }



                            thisDir["files"].push_back({
                                {"file_name",     WStringToUtf8(name)},
                                {"is_combined?",     false},
                                {"total_part",Totalpart },
                                {"parts", json::array()}
                                });
                        }
                    }
                } while (FindNextFile(hFind, &findData));
                FindClose(hFind);

                // Recurse into subdirectories if not OneStep
                if (!OneStep && !subDirs.empty()) {
                    Outline(subDirs, buffer, OneStep);
                }

                // Dump JSON and append to buffer (with comma if needed)
                string block = thisDir.dump();
                if (buffer.size() > 1) {
                    // previous content not just the initial '[' ? add comma separator
                    buffer.push_back(',');
                }
                buffer.insert(buffer.end(), block.begin(), block.end());
            }


        }
    }

    VOID verticalTraversal(const vector<wstring>& Paths, const std::vector<std::wstring>& extensions,
        const std::vector<std::wstring>& excludes) {
        for (auto& path : Paths) {
            if (isExcluded(path, excludes)) return;
            WIN32_FIND_DATAW fd;
            HANDLE hFind = INVALID_HANDLE_VALUE;

            wstring filePattern = path + L"\\*";
            hFind = FindFirstFileW(filePattern.c_str(), &fd);
            if (hFind == INVALID_HANDLE_VALUE) return;

            vector<wstring> subdirs;

            do {
                wstring name = fd.cFileName;
                if (name == L"." || name == L"..") continue;

                wstring fullPath = path + L"\\" + name;



                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    subdirs.push_back(fullPath);
                }
            } while (FindNextFileW(hFind, &fd) != 0);
            FindClose(hFind);

            for (const auto& sub : subdirs) {
                //vector<wstring> subvect = { sub };
                verticalTraversal({ sub }, extensions, excludes);
            }


            // Collect files after recursion
            hFind = FindFirstFileW(filePattern.c_str(), &fd);
            if (hFind == INVALID_HANDLE_VALUE) return;

            do {
                wstring name = fd.cFileName;
                if (name == L"." || name == L"..") continue;

                wstring fullPath = path + L"\\" + name;
                if ((!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) && (hasExtension(fullPath, extensions) && !isExcluded(fullPath, excludes))) {

                    //out.push_back(fullPath);  // full path saved
                    WIN32_FIND_DATA findFileData;
                    HANDLE hFind = FindFirstFileW(fullPath.c_str(), &findFileData);

                    if (hFind == INVALID_HANDLE_VALUE) {
                        //LOG
                        wcout << L"[INVALIDE] cant get handle in builde struct" << fullPath << endl;
                    }
                    LARGE_INTEGER fileSize;
                    fileSize.LowPart = findFileData.nFileSizeLow;
                    fileSize.HighPart = findFileData.nFileSizeHigh;

                    int size_PATH = WideCharToMultiByte(CP_UTF8, 0, &fullPath[0], (int)fullPath.size(), NULL, 0, NULL, NULL);

                    DWORD attributes = GetFileAttributes(fullPath.c_str());
                    if (attributes == INVALID_FILE_ATTRIBUTES) {
                        DWORD error_code = GetLastError();
                        wcerr << L"-Error (2) " << error_code << " " << fullPath << endl;
                        FindClose(hFind);
                        continue;
                    }
                    if ((attributes & FILE_ATTRIBUTE_READONLY) == 0) {

                        long long SizeFile = fileSize.QuadPart;
                        int Totalpart = SizeFile / CHUNK_SIZE;

                        if (SizeFile % CHUNK_SIZE != 0) {
                            Totalpart += 1;
                        }

                        int count = 0;
                        int seek = 0;

                        if (fileSize.QuadPart != 0) { // sizechunk
                            while (count < Totalpart) {


                                SendFileStruct* SendFileObject = new SendFileStruct;
                                SendFileObject->totalpart = Totalpart; // totalpart
                                SendFileObject->part = count + 1; // part

                                if (count == Totalpart - 1) { // sizechunk
                                    SendFileObject->sizechunk = SizeFile - seek;
                                }
                                else {
                                    SendFileObject->sizechunk = CHUNK_SIZE;
                                }
                                SendFileObject->sizepath = size_PATH; // sizepath
                                SendFileObject->filePath = fullPath; // filePath

                                seek += SendFileObject->sizechunk;
                                count++;
                                EnterCriticalSection(&csPrint);
                                sendFileQueue.push_back(SendFileObject); // append to Send queue
                                LeaveCriticalSection(&csPrint);
                            }

                        }


                    }
                    else {
                        DWORD error_code = GetLastError();
                        wcerr << L"-Error (3)  " << error_code << " " << fullPath << endl;
                        FindClose(hFind);
                        continue;
                    }
                    FindClose(hFind);
                }
            } while (FindNextFileW(hFind, &fd) != 0);
            FindClose(hFind);
        }
    }

    VOID horizontalTraversal(const vector<wstring>& Paths) {
        for (auto& path : Paths) {
            WIN32_FIND_DATAW fd;
            HANDLE hFind = INVALID_HANDLE_VALUE;

            wstring filePattern = path + L"\\*";
            hFind = FindFirstFileW(filePattern.c_str(), &fd);
            if (hFind == INVALID_HANDLE_VALUE) return;

            vector<wstring> subdirs;

            do {
                wstring name = fd.cFileName;
                if (name == L"." || name == L"..") continue;

                wstring fullPath = path + L"\\" + name;
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    subdirs.push_back(fullPath);
                }
                else {
                    //out.push_back(fullPath);  // full path saved
                    WIN32_FIND_DATA findFileData;
                    HANDLE hFind = FindFirstFileW(fullPath.c_str(), &findFileData);

                    if (hFind == INVALID_HANDLE_VALUE) {
                        //LOG
                    }
                    LARGE_INTEGER fileSize;
                    fileSize.LowPart = findFileData.nFileSizeLow;
                    fileSize.HighPart = findFileData.nFileSizeHigh;

                    int size_PATH = WideCharToMultiByte(CP_UTF8, 0, &fullPath[0], (int)fullPath.size(), NULL, 0, NULL, NULL);

                    DWORD attributes = GetFileAttributes(fullPath.c_str());
                    if (attributes == INVALID_FILE_ATTRIBUTES) {
                        DWORD error_code = GetLastError();
                        wcerr << L"-Error (2) " << error_code << " " << fullPath << endl;
                        FindClose(hFind);
                        continue;
                    }
                    if ((attributes & FILE_ATTRIBUTE_READONLY) == 0) {

                        long long SizeFile = fileSize.QuadPart;
                        int Totalpart = SizeFile / CHUNK_SIZE;

                        if (SizeFile % CHUNK_SIZE != 0) {
                            Totalpart += 1;
                        }

                        int count = 0;
                        int seek = 0;

                        if (fileSize.QuadPart != 0) { // sizechunk
                            while (count < Totalpart) {


                                SendFileStruct* SendFileObject = new SendFileStruct;
                                SendFileObject->totalpart = Totalpart; // totalpart
                                SendFileObject->part = count + 1; // part

                                if (count == Totalpart - 1) { // sizechunk
                                    SendFileObject->sizechunk = SizeFile - seek;
                                }
                                else {
                                    SendFileObject->sizechunk = CHUNK_SIZE;
                                }
                                SendFileObject->sizepath = size_PATH; // sizepath
                                SendFileObject->filePath = fullPath; // filePath

                                seek += SendFileObject->sizechunk;
                                count++;
                                EnterCriticalSection(&cs);
                                sendFileQueue.push_back(SendFileObject); // append to Send queue
                                LeaveCriticalSection(&cs);
                            }

                        }


                    }
                    else {
                        DWORD error_code = GetLastError();
                        wcerr << L"-Error (3)  " << error_code << " " << fullPath << endl;
                        FindClose(hFind);
                        continue;
                    }
                    FindClose(hFind);
                }
            } while (FindNextFileW(hFind, &fd) != 0);
            FindClose(hFind);

            for (auto& sub : subdirs) {
                //vector<wstring> subvect = { sub };
                horizontalTraversal({ sub });
            }
        }
    }

    bool compressChunk(const BYTE* input, size_t inputSize, BYTE*& output, size_t& outputSize) {
        outputSize = ZSTD_compressBound(inputSize);
        output = new BYTE[outputSize];
        size_t compressedSize = ZSTD_compress(output, outputSize, input, inputSize, 5);
        if (ZSTD_isError(compressedSize)) {
            delete[] output;
            output = nullptr;
            outputSize = 0;
            return false;
        }
        outputSize = compressedSize;
        return true;
    }

    BOOL Sender(SOCKET& sock, SendFileStruct& Part, int Task_id) {
        try
        {
            if (Part.Error12.Flag) {
                while (true) {
                    if (Part.Error12.isThreeSecondsPassed()) {
                        std::cout << "[Thread Sender] 3 sec have passed!\n";
                        Part.Error12.Flag = FALSE;
                        break;
                    }
                    else
                    {
                        Sleep(10);
                        continue;
                    }
                }
            }
            HANDLE hFile = CreateFile(
                Part.filePath.c_str(),
                GENERIC_READ,
                FILE_SHARE_READ,
                NULL,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                NULL
            );
            if (hFile == INVALID_HANDLE_VALUE) {
                wcout << "[INVALIDE] Handel error file name: " << Part.filePath.c_str() << endl;
                return TRUE;
                // LOG
            }
            BYTE* buffer = new BYTE[Part.sizechunk];
            DWORD bytesRead = 0;
            BYTE* compressedData = nullptr;
            int compressedSizeInt = 0;


            LONGLONG offset = (static_cast<long long>(Part.part) - 1) * CHUNK_SIZE;

            OVERLAPPED overlapped = { 0 };
            overlapped.Offset = static_cast<DWORD>(offset & 0xFFFFFFFF);
            overlapped.OffsetHigh = static_cast<DWORD>((offset >> 32) & 0xFFFFFFFF);


            BOOL readSuccess = ReadFile(hFile, buffer, Part.sizechunk, &bytesRead, &overlapped);

            if (!readSuccess) {
                DWORD err = GetLastError();
                if (err == ERROR_IO_PENDING) {
                    if (GetOverlappedResult(hFile, &overlapped, &bytesRead, TRUE)) {
                        cout << "Read " << bytesRead << " bytes at offset " << offset << endl;
                    }
                    else {
                        cerr << "GetOverlappedResult failed. Error: " << GetLastError() << endl;
                    }
                }
                else {
                    wstring a = Part.filePath.c_str();
                    wcout << L"ReadFile failed immediately. Error: " << err << L" file name: " << a << endl;
                }
            }
            else {
                if (bytesRead == 0) {
                    cout << "Reached end of file." << endl;
                }
                else {
                    size_t compressedSize = 0;
                    if (compressChunk(buffer, bytesRead, compressedData, compressedSize)) {
                        compressedSizeInt = static_cast<int>(compressedSize); // get compressed size
                    }
                    else {
                        std::cerr << "-Error (8)" << std::endl;
                    }
                }
            }
            CloseHandle(hFile);

            int sizeByteStream = 16 + Part.sizepath + compressedSizeInt; // 16 > 3 * int (totalpart, part,  sizechunk, size_PATH) +  data         // [*]
            BYTE* byteStream = new BYTE[sizeByteStream];
            BYTE* a = byteStream;
            // Build byteStream 
            memcpy(byteStream, &(Part.totalpart), sizeof(Part.totalpart));
            byteStream += sizeof(Part.totalpart);

            memcpy(byteStream, &(Part.part), sizeof(Part.part));
            byteStream += sizeof(Part.part);

            string FilePath(Part.sizepath, '\0');
            // Convert the file path to string (UTF-8)
            WideCharToMultiByte(CP_UTF8, 0, &Part.filePath[0], (int)Part.filePath.size(), &FilePath[0], Part.sizepath, NULL, NULL);

            // Copy size_PATH to the byte stream
            memcpy(byteStream, &Part.sizepath, sizeof(Part.sizepath));
            byteStream += sizeof(Part.sizepath);

            memcpy(byteStream, &(compressedSizeInt), sizeof(compressedSizeInt));// Copy sizechunk to the byte stream   // [*]
            byteStream += sizeof(compressedSizeInt);  // [*]

            memcpy(byteStream, &FilePath[0], Part.sizepath);                        // Copy file path to the byte stream
            byteStream += Part.sizepath;

            memcpy(byteStream, compressedData, compressedSizeInt);// [*]


            int CheckSendB = 0;
            int sentBytes = send(sock, (char*)(a), sizeByteStream, 0); // send ByteStream
            if (sentBytes == SOCKET_ERROR) {
                //EnterCriticalSection(&csPrint);
                cout << "[Thread Sender] Failed to send part." << endl;
                return FALSE;
            }

            //CheckSendB += sentBytes;
            EnterCriticalSection(&csPrint);
            totalBytesSent.QuadPart += sentBytes;
            LeaveCriticalSection(&csPrint);
            delete[] buffer;
            delete compressedData;
            delete[] a;

            char bufferAKCs[4];
            int received = 0;

            while (received < 4) {
                int bytes = recv(sock, bufferAKCs + received, 4 - received, 0);
                if (bytes == SOCKET_ERROR) {
                    int err = WSAGetLastError();
                    if (err == WSAETIMEDOUT) {
                        std::wcout << "[Thread Sender] Timeout Error 12.\n";
                        SOCKET sock = UL_Socket::ConnectToServer(IP, PORT);
                        UL_Socket::SocketInitialization(sock, 1113);

                        int sizebyteStreamError12 = 12 + Part.sizepath;
                        BYTE* byteStreamError12 = new BYTE[sizebyteStreamError12];
                        BYTE* a = byteStreamError12;
                        memcpy(byteStreamError12, &(Part.part), sizeof(Part.part));
                        byteStreamError12 += sizeof(Part.part);
                        memcpy(byteStreamError12, &(Task_id), sizeof(Task_id));
                        byteStreamError12 += sizeof(Task_id);
                        memcpy(byteStreamError12, &Part.sizepath, sizeof(Part.sizepath));
                        byteStreamError12 += sizeof(Part.sizepath);
                        memcpy(byteStreamError12, &FilePath[0], Part.sizepath);                        // Copy file path to the byte stream
                        byteStreamError12 += Part.sizepath;

                        int sentBytes = send(sock, (char*)(a), sizebyteStreamError12, 0); // send ByteStream

                        Part.Error12.Flag = TRUE;
                        Part.Error12.set();
                        //connected.store(false);
                        //closesocket(sock);
                        delete[] a;
                        return FALSE;
                    }
                    else {
                        //EnterCriticalSection(&csPrint);
                        return FALSE;
                        //LeaveCriticalSection(&csPrint);
                    }
                }
                else if (bytes == 4) {
                    // Convert 4 bytes to an integer
                    int receivedValue;
                    memcpy(&receivedValue, bufferAKCs, sizeof(receivedValue));
                    //EnterCriticalSection(&csPrint);
                    if (receivedValue == Part.part) {
                        return TRUE;
                    }
                    else {
                        //EnterCriticalSection(&csPrint);
                        wcout << "[Thread Sender] Acknowledg Not match" << endl;
                        //LeaveCriticalSection(&csPrint);
                        return FALSE;
                    }
                    //LeaveCriticalSection(&csPrint);
                }
                else {
                    //EnterCriticalSection(&csPrint);
                    wcout << "[Thread Sender] Acknowledg Failed" << endl;
                    //LeaveCriticalSection(&csPrint);
                    return FALSE;
                }
                received += bytes;
            }

        }
        catch (const std::exception& e) {
            std::cout << "[Thread Sender] catch" << e.what() << std::endl;

            return FALSE;
        }

    }

    bool poll_recv(SOCKET sock, int timeout_ms) {
        WSAPOLLFD fd;
        fd.fd = sock;
        fd.events = POLLRDNORM;

        int result = WSAPoll(&fd, 1, timeout_ms);
        return (result > 0 && (fd.revents & POLLRDNORM));
    }

    DWORD WINAPI Set_idx_and_callSender(LPVOID lpParam) {
        ThreadArgs* args = static_cast<ThreadArgs*>(lpParam);
        int threadID = args->i;
        int Task_id = args->Task_id;

        atomic<bool>* runningFlag = threadRunningFlags[threadID];

        SOCKET sock;
        do
        {
            sock = UL_Socket::ConnectToServer(IP, PORT);
        } while (sock == INVALID_SOCKET);

        int result;
        do
        {
            result = UL_Socket::SocketInitialization(sock, 3247);
        } while (result != TRUE);

        vector<char> buffer;
        buffer.insert(buffer.end(), reinterpret_cast<char*>(&Task_id), reinterpret_cast<char*>(&Task_id) + sizeof(int));
        int SentFlagAndId = send(sock, buffer.data(), buffer.size(), 0); // static id

        while (globalRunning && *runningFlag) {

            SendFileStruct* part = nullptr;
            EnterCriticalSection(&cs);
            if (!sendFileQueue.empty()) {
                part = sendFileQueue.front();
                sendFileQueue.pop_front();
            }
            LeaveCriticalSection(&cs);
            if (sendFileQueue.empty() && (part == nullptr)) {
                if (Flagendbuildstruct.load()) {
                    globalRunning = FALSE;
                    break;
                }
                Sleep(1500);
                continue;
            }

            if (Sender(sock, *part, Task_id)) {
                // LOG
                delete part;
            }
            else
            {
                EnterCriticalSection(&cs);
                sendFileQueue.push_back(part);
                LeaveCriticalSection(&cs);
                if (!UL_Socket::IsSocketAlive(sock)) {
                    cout << "point 1" << endl;
                    break;
                }
            }
        }

        closesocket(sock);
        EnterCriticalSection(&cs);
        cout << "Thread " << threadID << " exiting.\n";
        LeaveCriticalSection(&cs);
        return 0;
    }

    DWORD WINAPI ManagerThread(LPVOID) {
        while (globalRunning) {
            Sleep(2000);

            // 1) grab lock and decide what to do
            EnterCriticalSection(&cs);
            int currentCount = (int)threadHandles.size();
            int targetCount = desiredThreadCount.load();

            if (currentCount > targetCount) {
                cout << "Manager: Reducing threads from "
                    << currentCount << " to " << targetCount << "\n";

                // collect handles & flags to shut down
                vector<HANDLE>             toCloseHandles;
                vector<atomic<bool>*> toDeleteFlags;

                // signal and remove from the shared vectors under lock
                for (int i = currentCount - 1; i >= targetCount; --i) {
                    // signal thread to stop
                    *threadRunningFlags[i] = false;
                    // stash for cleanup
                    toCloseHandles.push_back(threadHandles[i]);
                    toDeleteFlags.push_back(threadRunningFlags[i]);
                    // remove from shared vectors
                    threadHandles.pop_back();
                    threadRunningFlags.pop_back();
                }
                LeaveCriticalSection(&cs);

                // now outside the lock we wait & delete
                for (HANDLE h : toCloseHandles) {
                    WaitForSingleObject(h, INFINITE);
                    CloseHandle(h);
                }
                for (auto f : toDeleteFlags) {
                    delete f;
                }
                continue;
            }
            else if (currentCount < targetCount) {
                cout << "Manager: Increasing threads from "
                    << currentCount << " to " << targetCount << "\n";

                for (int i = currentCount; i < targetCount; ++i) {
                    auto* runningFlag = new atomic<bool>(true);
                    threadRunningFlags.push_back(runningFlag);

                    HANDLE hThread = CreateThread(
                        nullptr, 0,
                        Set_idx_and_callSender,
                        (LPVOID)(INT_PTR)i,
                        0, nullptr
                    );
                    if (hThread == nullptr) {
                        cerr << "Failed to create thread " << i
                            << " (err=" << GetLastError() << ")\n";
                        delete runningFlag;
                    }
                    else {
                        threadHandles.push_back(hThread);
                    }
                }
            }

            LeaveCriticalSection(&cs);
        }

        return 0;
    }

    SOCKET tryConnect() {
        WSADATA wsa;
        SOCKET sock;
        struct sockaddr_in server;

        WSAStartup(MAKEWORD(2, 2), &wsa);
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == INVALID_SOCKET)
            return INVALID_SOCKET;

        // ✅ Set receive timeout
        DWORD timeout = 10000;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

        server.sin_family = AF_INET;
        server.sin_port = htons(SERVER_PORT);
        inet_pton(AF_INET, IP, &server.sin_addr);

        if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
            closesocket(sock);
            return INVALID_SOCKET;
        }

        return sock;
    }

    void workerThread(int id, int Task_id) {
        SOCKET sock = INVALID_SOCKET;

        while (!stopAll.load()) {
            while (sock == INVALID_SOCKET && !stopAll.load()) {
                //sock = UL_Socket::ConnectToServer(IP, PORT);
                sock = tryConnect();
                if (sock == INVALID_SOCKET) {
                    EnterCriticalSection(&csPrint);
                    std::cout << "[Thread " << id << "] Connection failed. Retrying in 5s...\n";
                    LeaveCriticalSection(&csPrint);
                    Sleep(5000);
                }
                else {
                    EnterCriticalSection(&csPrint);
                    std::cout << "[Thread " << id << "] Connected.\n";
                    LeaveCriticalSection(&csPrint);
                }
            }
            int result;

            result = UL_Socket::SocketInitialization(sock, 3247);


            vector<char> buffer;
            buffer.insert(buffer.end(), reinterpret_cast<char*>(&Task_id), reinterpret_cast<char*>(&Task_id) + sizeof(int));
            int SentFlagAndId = send(sock, buffer.data(), buffer.size(), 0); // static id



            while (!stopAll.load()) {
                SendFileStruct* part = nullptr;
                if (ReceivedVolumeByTime.QuadPart) { // for argument   -Time   -volume
                    if (totalBytesSent.QuadPart > ReceivedVolumeByTime.QuadPart * 1024 * 1024 * 1024)
                        Sleep(timer * 60 * 1000);
                    totalBytesSent.QuadPart = 0;
                    cout << "LOG time sleep";
                }
                EnterCriticalSection(&csPrint);

                if (!sendFileQueue.empty()) {
                    part = sendFileQueue.front();
                    sendFileQueue.pop_front();
                }
                else {
                    closesocket(sock);
                    //connected.store(true);
                    stopAll.store(true);
                    LeaveCriticalSection(&csPrint);
                    return;
                }
                LeaveCriticalSection(&csPrint);
                //if (sendFileQueue.empty() && (part == nullptr)) {
                //    closesocket(sock);
                //    stopAll.store(true);
                //    return;
                //}
                if (Sender(sock, *part, Task_id)) {
                    // LOG
                    //EnterCriticalSection(&csPrint);
                    //cout << "[Sender] Part sended" << endl;
                    delete part;
                    //LeaveCriticalSection(&csPrint);

                }
                else
                {

                    EnterCriticalSection(&csPrint);
                    sendFileQueue.push_back(part); // Retry later
                    LeaveCriticalSection(&csPrint);


                    closesocket(sock);
                    sock = INVALID_SOCKET; // Will trigger reconnect
                    break; // Exit to reconnect loop
                }

                //Sleep(100); // throttle reconnect loops

                //EnterCriticalSection(&csPrint);
                //std::cout << "[Thread " << id << "] Sent chunk " << idx << " (" << sent << " bytes)\n";
                //LeaveCriticalSection(&csPrint);
                //Sleep(10);
            }

        }
        if (sock != INVALID_SOCKET)
            closesocket(sock);
    }

    void monitorThread() {
        while (!stopAll.load()) {
            if (!connected.load()) {
                SOCKET s = tryConnect();
                if (s != INVALID_SOCKET) {
                    EnterCriticalSection(&csPrint);
                    std::cout << "[Monitor] Server is back. Resuming threads.\n";
                    LeaveCriticalSection(&csPrint);
                    closesocket(s);
                    connected.store(true);
                }
                else {
                    EnterCriticalSection(&csPrint);
                    std::cout << "[Monitor] Server still unreachable. Retrying in 5s...\n";
                    LeaveCriticalSection(&csPrint);
                }
            }

            if (sendFileQueue.empty()) {
                connected.store(true);
                stopAll.store(true);
                break;
            }

            Sleep(5000);
        }
    }
}