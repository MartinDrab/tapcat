
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>
#include <winioctl.h>










#define TAP_CONTROL_CODE(request,method)		CTL_CODE(FILE_DEVICE_UNKNOWN, request, method, FILE_ANY_ACCESS)
#define TAP_IOCTL_CONFIG_TUN					TAP_CONTROL_CODE(10, METHOD_BUFFERED)
#define TAP_IOCTL_SET_MEDIA_STATUS				TAP_CONTROL_CODE(6, METHOD_BUFFERED)

#define TAP_ADAPTER_KEY							"SYSTEM\\CurrentControlSet\\Control\\Class\\{4D36E972-E325-11CE-BFC1-08002BE10318}"
#define NETWORK_KEY								"SYSTEM\\CurrentControlSet\\Control\\Network\\{4D36E972-E325-11CE-BFC1-08002BE10318}"
#define TAP_DEVICE_SPACE						"\\\\.\\Global\\"
#define TAP_VERSION_ID_0801						"tap0801"
#define TAP_VERSION_ID_0901						"tap0901"
#define KEY_COMPONENT_ID						"ComponentId"
#define NET_CFG_INST_ID							"NetCfgInstanceId"



static int
get_name(char *ifname, int namelen, const char *dev_name)
{
	char path[256];
	LONG status;
	HKEY conn_key;
	DWORD datatype;

	memset(ifname, 0, namelen);
	snprintf(path, sizeof(path), NETWORK_KEY "\\%s\\Connection", dev_name);
	status = RegOpenKeyExA(HKEY_LOCAL_MACHINE, path, 0, KEY_QUERY_VALUE, &conn_key);
	if (status != ERROR_SUCCESS) {
		fprintf(stderr, "Could not look up name of interface %s: error opening key\n", dev_name);
		return status;
	}

	memset(ifname, 0, namelen);
	namelen -= sizeof(char);
	status = RegQueryValueExA(conn_key, "Name", NULL, &datatype, (LPBYTE)ifname, &namelen);
	if (status != ERROR_SUCCESS || datatype != REG_SZ) {
		if (status == ERROR_SUCCESS)
			status = ERROR_INVALID_PARAMETER;
		
		fprintf(stderr, "Could not look up name of interface %s: error reading value\n", dev_name);
		RegCloseKey(conn_key);
		goto CloseKey;
	}

CloseKey:
	RegCloseKey(conn_key);

	return status;
}


static int
get_device(char *DeviceBuffer, int DeviceBufferSize)
{
	int index = 0;
	HKEY adapter_key = NULL;
	LONG status = ERROR_GEN_FAILURE;

	memset(DeviceBuffer, 0, DeviceBufferSize);
	status = RegOpenKeyExA(HKEY_LOCAL_MACHINE, TAP_ADAPTER_KEY, 0, KEY_ENUMERATE_SUB_KEYS, &adapter_key);
	if (status == ERROR_SUCCESS) {
		while (status == ERROR_SUCCESS && *DeviceBuffer == '\0') {
			char name[256];
			char component[256];
			HKEY device_key;
			DWORD datatype;
			DWORD len;

			len = sizeof(name) / sizeof(name[0]);
			status = RegEnumKeyExA(adapter_key, index, name, &len, NULL, NULL, NULL, NULL);
			if (status == ERROR_SUCCESS) {
				status = RegOpenKeyExA(adapter_key, name, 0, KEY_QUERY_VALUE, &device_key);
				if (status == ERROR_SUCCESS) {
					len = sizeof(component) - sizeof(char);
					memset(component, 0, sizeof(component));
					status = RegQueryValueExA(device_key, KEY_COMPONENT_ID, NULL, &datatype, (LPBYTE)component, &len);
					if (status == ERROR_SUCCESS && datatype == REG_SZ &&
						(strcmp(TAP_VERSION_ID_0801, component) == 0 ||
							strcmp(TAP_VERSION_ID_0901, component) == 0)) {
						len = DeviceBufferSize - sizeof(char);
						memset(DeviceBuffer, 0, DeviceBufferSize);
						status = RegQueryValueExA(device_key, NET_CFG_INST_ID, NULL, &datatype, (LPBYTE)DeviceBuffer, (DWORD *)&len);
						if (status == ERROR_SUCCESS && datatype != REG_SZ)
							*DeviceBuffer = '\0';
					}

					RegCloseKey(device_key);
				}

				status = ERROR_SUCCESS;
			}

			index++;
		}

		RegCloseKey(adapter_key);
	}

	return status;
}


static char *
create_cmdline(int argc, const char *argv[])
{
	size_t argLen = 0;
	char *tmp = NULL;
	char *ret = NULL;
	size_t totalSize = 0;

	for (int i = 0; i < argc; ++i)
		totalSize += (strlen(argv[i]) + 3*sizeof(char));

	ret = calloc(totalSize, sizeof(char));
	if (ret != NULL) {
		tmp = ret;
		for (int i = 0; i < argc; ++i) {
			if (tmp != ret)
				*(tmp - 1) = ' ';

			argLen = strlen(argv[i]);
			memcpy(tmp + 1, argv[i], argLen);
			tmp[0] = '"';
			tmp[argLen + 1] = '"';
			tmp += (argLen + 3);
		}
	}

	return ret;
}


int
open_tap(const char *dev, int *device_handle)
{
	char adapter[256];
	char tapfile[512];
	int status = ERROR_GEN_FAILURE;
	HANDLE tmpDeviceHandle = NULL;

	memset(adapter, 0, sizeof(adapter));
	status = get_device(adapter, sizeof(adapter));
	if (status == ERROR_SUCCESS) {
		snprintf(tapfile, sizeof(tapfile), "%s%s.tap", TAP_DEVICE_SPACE, adapter);
		fprintf(stderr, "Opening device %s\n", tapfile);
		tmpDeviceHandle = CreateFileA(tapfile, GENERIC_WRITE | GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_SYSTEM | FILE_FLAG_OVERLAPPED, NULL);
		if (tmpDeviceHandle != INVALID_HANDLE_VALUE) {
			*device_handle = (int)tmpDeviceHandle;
		} else status = GetLastError();
	}

	return status;
}


void
close_tap(int device_handle)
{
	CloseHandle((HANDLE)device_handle);

	return;
}


int 
execute_process(int argc, const char *argv[], int TAPHandle)
{
	char *cmdLine = NULL;
	int ret = ERROR_GEN_FAILURE;
	STARTUPINFOA si;
	PROCESS_INFORMATION pi;

	ret = ERROR_SUCCESS;
	cmdLine = create_cmdline(argc, argv);
	if (cmdLine != NULL) {
		memset(&si, 0, sizeof(si));
		si.cb = sizeof(si);
		si.dwFlags = STARTF_USESTDHANDLES;
		si.hStdInput = (HANDLE)TAPHandle;
		si.hStdOutput = (HANDLE)TAPHandle;
		si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
		if (!SetHandleInformation(si.hStdInput, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT))
			ret = GetLastError();
		
		if (ret == ERROR_SUCCESS && !SetHandleInformation(si.hStdError, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT))
			ret = GetLastError();
		
		if (ret == ERROR_SUCCESS && CreateProcessA(argv[0], cmdLine, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
			close_tap(TAPHandle);
			CloseHandle(pi.hThread);
			WaitForSingleObject(pi.hProcess, INFINITE);
			CloseHandle(pi.hProcess);
		} else ret = GetLastError();
	} else ret = ERROR_NOT_ENOUGH_MEMORY;

	return ret;
}
