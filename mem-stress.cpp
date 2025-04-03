#include <iostream>
#include <cstring>
#include <random>
#include <thread>
#include <chrono>
#include <mutex>
#include <vector>
#include "sys/sysinfo.h"

// Buffer que vai alocar a memoria do programa, volatile para evitar que o compilador otimize a leitura/escrita
volatile char * buffer = nullptr;
std::mutex bufferMutex;

// Extrai o numero de uma linha do arquivo como "VmSize: 12345 kB"
int parseLine(char* line){
    int i = strlen(line);
    const char* p = line;
    while (*p <'0' || *p > '9') p++;
    line[i-3] = '\0';
    i = atoi(p);
    return i;
}

// Busca o uso de memoria do processo atual em KB
int getProcessMemUsage()
{
    // pega arquivo do /proc/self, que contem detalhes do processo do Linux
    FILE* file = fopen("/proc/self/status", "r");
    int result = -1;
    char line[128];

    // Le linha por lnha ate achar o VmSize (Virtua memory)
    while (fgets(line, 128, file) != NULL){
        if (strncmp(line, "VmSize:", 7) == 0){
            result = parseLine(line);
            break;
        }
    }
    fclose(file);
    return result;
}

long long getTotalAvailablePhysicalMemory()
{
  // cria uma variavel do tipo sysinfo
  struct sysinfo memInfo;

  // popula a veriavel com os dados do sistema
  sysinfo(&memInfo);

  // inicializa com o total de ram fisica disponivel
  long long totalPhysicalMem = memInfo.freeram;
  totalPhysicalMem *= memInfo.mem_unit; // converte para bytes

  return totalPhysicalMem;
}

long long calculateBufferSize(int percentLimit)
{
  long long totalAvailablePhysicalMem = getTotalAvailablePhysicalMemory();
  return (totalAvailablePhysicalMem * percentLimit) / 100;
}

void stressMemoryThread(std::chrono::time_point<std::chrono::steady_clock> finishTime, long long bufferSize)
{
  std::random_device randomDevice;
  std::mt19937_64 generator(randomDevice());
  std::uniform_int_distribution<long long> distMemPosition(0, bufferSize - 1);

  while (finishTime > std::chrono::steady_clock::now())
  {
    std::lock_guard<std::mutex> guard(bufferMutex);

    long long memoryPosition = distMemPosition(generator);
    char dataInMemory = buffer[memoryPosition];
    buffer[memoryPosition] = (char) (rand() % 256);
  }
}

int main()
{
  int qtyCores = 8;
  int percentLimit = 35;
  int minutesToRun = 1;

  long long bufferSize = calculateBufferSize(percentLimit);

  try
  {
    buffer = new char[bufferSize];
    // memset preenche x espacos do bloco de memoria com caracteres aleatorios
    memset((void*)buffer, 0, bufferSize); 
  }
  catch (const std::bad_alloc& e)
  {
    std::cerr << "Memory allocation failed: " << e.what() << std::endl;
    return 1;
  }

  std::chrono::time_point finishTime = std::chrono::steady_clock::now() + std::chrono::minutes(minutesToRun);

  std::vector<std::thread> threads;

  // Aloca 2 threads por nucleo
  for (int i = 0; i < std::thread::hardware_concurrency() * 2; i++)
  {
    threads.push_back(std::thread(stressMemoryThread, finishTime, bufferSize));
  }

  // Impede que o programa feche antes das threads finalizarem
  for (auto& thread : threads) {
    thread.join(); 
  }

  delete[] buffer;
  return 0;
}
