// FUNCTION.h
#ifndef FUNCTION_H
#define FUNCTION_H

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iostream>
#include <thread>
#include <vector>
#include <queue>
#include <chrono>
#include <future>
#include <atomic>
#include <mutex>
#include <zstd.h> 
#include <sstream>
#include "json.hpp"
#include <iomanip>
#include <lm.h>
#include <string>
#include "logger.h"
#include <deque>
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Netapi32.lib")
using json = nlohmann::json;
using namespace std;

struct ThreadArgs {
	int i;
	int Task_id; // or any other types you want to pass
};
struct ThreadParams {
	int Task_flag;
	int Task_id;
	vector<wstring> vecWstr;
};
struct ThreadParamsForCase4 {
	int Task_flag;
	int Task_id;
	vector<wstring> vecWstr;
	string vector_pathes;
	bool resumeFlag;
	vector<wstring> vector_Excludes;
	vector<wstring> vector_Extensions;
};
struct Error12 {
	std::chrono::steady_clock::time_point start_time;
	BOOL Flag = FALSE;

	void set() {
		start_time = std::chrono::steady_clock::now();
	}

	bool isThreeSecondsPassed() const {
		auto now = std::chrono::steady_clock::now();
		auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time);
		return elapsed.count() >= 3;
	}
};
struct Task {
	void (*func)(void*);
	std::string* json_item;
};
struct SendFileStruct {
	int totalpart = 0;
	int part = 0;
	int sizechunk = 0;
	int sizepath = 0;
	std::wstring filePath = L"";
	Error12 Error12;
};
extern SOCKET                             sockctr;
extern const char* IP;
extern const char* PORT;
extern BOOL                                RunOnce;
extern CRITICAL_SECTION                   cs;
extern LONG                               CHUNK_SIZE;
extern vector<HANDLE>                     threadHandles;
extern vector<atomic<bool>*>              threadRunningFlags;
extern atomic<int>                        desiredThreadCount;
extern atomic<bool>                       globalRunning;
extern atomic<bool>                       start_flag;
extern atomic<bool>                       Flagendbuildstruct;
extern LARGE_INTEGER                      ReceivedVolumeByTime;
extern DWORD                              timer;
extern DWORD                              MehtodFlag;
extern vector<HANDLE>                     TaskThreads;
extern LONG                               g_nextIndex;
extern deque<SendFileStruct*>             sendFileQueue;
extern LARGE_INTEGER                      totalBytesSent;
extern vector<char>                       g_jsonBuffer;
extern int count;
extern queue<Task>                        taskQueue;


namespace UL_Socket {
	BOOL          SocketInitialization(SOCKET& sock, int Flag);
	bool IsSocketAlive(SOCKET sock);
	SOCKET        ConnectToServer(const char* serverAddress, const char* port);
	BOOL recvWithSize(SOCKET sock, vector<char>& buffer, int size);

}
namespace UL_SocketTools {
	VOID TaskProcessor(void* Task_Json);
	DWORD WINAPI TaskManager(LPVOID lpParam);
	BOOL         SettingMainVariables(SOCKET& sock);
	INT          extractAndRemoveFlag(vector<char>& buffer, size_t sizeofByte);
	DWORD WINAPI Trap(LPVOID lpParam);
	BOOL SendSizeFlagAndBuffer_withoutSizeFlag(SOCKET sock, int TaskId, int bufferSize, BYTE* buffer);
	BOOL         SendSizeFlagAndBuffer(SOCKET sock, int TaskId, const vector<char>& buffer);
}
namespace UL_win32Tools {
	wstring		 utf8_to_wstring(const string& str);
	string		 wstring_to_utf8(const wstring& wstr);
	void		 add_logical_drives(json& resultJson);
	void		 add_shared_folders(json& resultJson);
	vector<char> getDrivesAndSharedFolders();
	string		 FileTimeToString(FILETIME ft);
	void		 TraverseDirectory(const vector<wstring>& folders, vector<char>& buffer, BOOL OneStep);
}
namespace UL_SwitchCase {
	DWORD WINAPI Case1_LoadDriveAndShare(LPVOID lpParam);
	DWORD WINAPI Case2_LoadComplete(LPVOID lpParam);
	DWORD WINAPI Case3_LoadLayer(LPVOID lpParam);
	DWORD WINAPI Case4_UpLoad(LPVOID lpParam);
}
namespace UL_JsonTools {
	VOID split_and_parse_json_objects(const string& input);
	json         parseBufferToJson(vector<char>* buffer);
}
namespace UL_Sender {
	VOID resumeBuildStruct(wstring& PATH, wstring& file_name, int& total_part, vector<int> parts);
	std::string WStringToUtf8(const std::wstring& wstr);
	std::wstring GetRelativePath(const std::wstring& full, const std::wstring& root);
	bool startsWith(const std::wstring& fullPath, const std::wstring& prefix);
	bool isExcluded(const std::wstring& path, const std::vector<std::wstring>& excludedDirs);
	bool hasExtension(const std::wstring& filename, const std::vector<std::wstring>& extensions);
	std::string fileTimeToISO(const FILETIME& ft);
	void findFilesGroupedByFolderToBuffer(const std::wstring& folderPath,
		const std::vector<std::wstring>& extensions,
		const std::vector<std::wstring>& excludes,
		std::vector<char>& buffer);
	void collectJsonFolderBlobs(const std::vector<std::wstring>& roots,
		const std::vector<std::wstring>& extensions,
		const std::vector<std::wstring>& excludes,
		std::vector<char>& outputBuffer);
	VOID SendFlagUploadEnd(int TaskId);
	VOID horizontalTraversal(const vector<wstring>& Paths);
	VOID verticalTraversal(const vector<wstring>& Paths, const std::vector<std::wstring>& extensions,
		const std::vector<std::wstring>& excludes);
	BOOL Sender(SOCKET& sock, SendFileStruct& Part, int Task_id);
	bool poll_recv(SOCKET sock, int timeout_ms);
	DWORD WINAPI Set_idx_and_callSender(LPVOID lpParam);
	DWORD WINAPI ManagerThread(LPVOID);
	SOCKET tryConnect();
	void workerThread(int id, int Task_id);
	void monitorThread();
}
#endif // FUNCTION_H
