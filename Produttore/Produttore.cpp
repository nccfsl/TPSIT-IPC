#include "pch.h"
#include <Windows.h>
#include <stdio.h>

#define BLOCK_SIZE 1024

#pragma warning ( disable : 4996 )
#pragma warning ( suppress: 4267 )

struct SHARED
{
	unsigned char buffer[BLOCK_SIZE];
	unsigned int count;
	int end;
};

STARTUPINFO startup_window;
PROCESS_INFORMATION child_process;

int main(int argc, char* argv[])
{
	struct SHARED *shared_data;
	HANDLE shared_map, empty_semaphore, full_semaphore;
	FILE* input_file;
	long count;
	char command[1024];

	if (argc != 3)
	{
		printf("Uso: %s input-file output-file\r\n", argv[0]);
		return -1;
	}

	shared_map = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(struct SHARED), (LPWSTR)"SHARED");

	if (shared_map == NULL)
	{
		printf("Errore creazione memory-map\r\n");
		return -1;
	}

	shared_data = (struct SHARED*)MapViewOfFile(shared_map, FILE_MAP_WRITE, 0, 0, sizeof(struct SHARED));

	if (shared_data == NULL)
	{
		printf("Errore associazione memory-map\r\n");
		CloseHandle(shared_map);
		return -1;
	}

	shared_data->count = 0;
	shared_data->end = FALSE;

	empty_semaphore = CreateSemaphore(NULL, 1, 1, (LPWSTR)"EMPTY");

	if (empty_semaphore == NULL)
	{
		printf("Errore creazione semaforo EMPTY\r\n");
		UnmapViewOfFile(shared_data);
		CloseHandle(shared_map);
		return -1;
	}

	full_semaphore = CreateSemaphore(NULL, 0, 1, (LPWSTR)"FULL");

	if (full_semaphore == NULL)
	{
		printf("Errore creazione semaforo FULL\r\n");
		UnmapViewOfFile(shared_data);
		CloseHandle(shared_map);
		CloseHandle(empty_semaphore);
		return -1;
	}

	input_file = fopen(argv[1], "rb");

	if (input_file == NULL)
	{
		printf("Errore apertura file %s\r\n", argv[1]);
		UnmapViewOfFile(shared_data);
		CloseHandle(shared_map);
		CloseHandle(empty_semaphore);
		CloseHandle(full_semaphore);
		return -1;
	}

	strcpy(command, argv[0]);
	strcat(command, " ");
	strcat(command, argv[2]);

	if (CreateProcess((LPWSTR)"Consumer.exe", (LPWSTR)command, NULL, NULL, TRUE, 0, NULL, NULL, &startup_window, &child_process) == 0)
	{
		printf("Errore nella creazione processo consumatore\r\n");
		UnmapViewOfFile(shared_data);
		CloseHandle(shared_map);
		CloseHandle(empty_semaphore);
		CloseHandle(full_semaphore);
		fclose(input_file);
		return -1;
	}

	do
	{
		WaitForSingleObject(empty_semaphore, INFINITE);

		shared_data->count = fread(shared_data->buffer, 1, BLOCK_SIZE, input_file);
		if (feof(input_file))
			shared_data->end = TRUE;
		ReleaseSemaphore(full_semaphore, 1, &count);
	} 
	while (!feof(input_file));

	fclose(input_file);
	CloseHandle(empty_semaphore);
	CloseHandle(full_semaphore);
	UnmapViewOfFile(shared_data);
	CloseHandle(shared_map);

	return 0;
}
