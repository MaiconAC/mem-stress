#include <iostream>
#include <cstring>
#include <random>
#include <thread>
#include <chrono>
#include <mutex>
#include <vector>
#include "libs/CLI11.hpp"
#include "sys/sysinfo.h"

// Buffer que vai alocar a memoria do programa, volatile para evitar que o compilador otimize a leitura/escrita
volatile char * buffer = nullptr;
std::mutex bufferMutex;

// Contador de erros nas operacoes de memoria
int errCounter = 0;

// Extrai o numero de uma linha do arquivo como "VmSize: 12345 kB"
int parseLine(char* line)
{
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

long long getTotalAvailableVirtualMemory()
{
    // cria uma variavel do tipo sysinfo
    struct sysinfo memInfo;

    // popula a veriavel com os dados do sistema
    sysinfo(&memInfo);

    // inicializa com o total de memoria disponivel
    long long totalMem = memInfo.freeram + memInfo.freeswap;
    totalMem *= memInfo.mem_unit; // converte para bytes

    return totalMem;
}

long long calculateBufferSize(int percentLimit)
{
    long long totalAvailablePhysicalMem = getTotalAvailableVirtualMemory();
    return (totalAvailablePhysicalMem * percentLimit) / 100;
}

void invertBinaryValueThread(std::chrono::time_point<std::chrono::steady_clock> finishTime, long long bufferSize)
{
    std::random_device randomDevice;
    std::mt19937_64 memPositionGenerator(randomDevice());
    std::uniform_int_distribution<long long> memPositionDistribution(0, bufferSize - 1);

    while (finishTime > std::chrono::steady_clock::now())
    {
        std::lock_guard<std::mutex> guard(bufferMutex);

        long long memoryPosition = memPositionDistribution(memPositionGenerator);

        int oldData = buffer[memoryPosition];

        // Operador ~ inverte o valor binario
        buffer[memoryPosition] = ~buffer[memoryPosition];

        if (buffer[memoryPosition] != ~oldData)
        {
            std::cout << "Error while inverting binary" << std::endl;
            errCounter++;
        }
    }
}

void swapValuesThread(std::chrono::time_point<std::chrono::steady_clock> finishTime, long long bufferSize)
{
    std::random_device randomDevice;
    std::mt19937_64 memPositionGenerator(randomDevice());
    std::uniform_int_distribution<long long> memPositionDistribution(0, bufferSize - 1);

    while (finishTime > std::chrono::steady_clock::now())
    {
        std::lock_guard<std::mutex> guard(bufferMutex);

        long long firstMemoryPosition = memPositionDistribution(memPositionGenerator);
        long long secondMemoryPosition = memPositionDistribution(memPositionGenerator);

        char firstDataInMemory = buffer[firstMemoryPosition];
        char secondDataInMemory = buffer[secondMemoryPosition];

        // Operador ~ inverte o valor binario
        buffer[firstMemoryPosition] = secondDataInMemory;
        buffer[secondMemoryPosition] = firstDataInMemory;

        if (buffer[firstMemoryPosition] != secondDataInMemory || buffer[secondMemoryPosition] != firstDataInMemory)
        {
            std::cout << "Error while swapping values" << std::endl;
            errCounter++;
        }
    }
}

void writePattern(long long startIndex, long long finalIndex)
{

    for (long long i = startIndex; i < finalIndex; i++)
    {
        // std::lock_guard<std::mutex> guard(bufferMutex);
        buffer[i] = i % 2 == 0 ? 0x55 : 0xAA;
    }
}

void fillBuffer(long long bufferSize, int qtyCores)
{
    std::vector<std::thread> threads;

    long long sizeToFill = bufferSize / (qtyCores * 2);

    std::cout << "Memory to fill each thread: " << sizeToFill << std::endl;

    for (int i = 0; i < qtyCores * 2; i++)
    {
        long long startIndex = i * sizeToFill;

        // Se for a ultima thread, preenche ate o final
        long long finalIndex = i == (qtyCores * 2 - 1)
            ? bufferSize
            :  (i + 1) * sizeToFill;

        threads.push_back(std::thread(writePattern, startIndex, finalIndex));
    }

    // Impede que o programa feche antes das threads finalizarem
    for (auto& thread : threads) {
        thread.join();
    }
}

int main(int argc, char **argv)
{
    CLI::App app;

    int qtyCores{1};
    app.add_option("--cores", qtyCores, "Quantity of cores to stress");

    int percentLimit{60};
    app.add_option("--perc", percentLimit, "Percentage limit of memory use");

    int minutesToRun{1};
    app.add_option("--min", minutesToRun, "Minutes to run");

    CLI11_PARSE(app, argc, argv);

    std::cout << "Memory Stresser started!" << std::endl;
    std::cout << "CPU cores to use: " << qtyCores << std::endl;
    std::cout << "Mem use limit (%): " << percentLimit << std::endl;
    std::cout << "Minutes to run: " << minutesToRun << std::endl;

    if (qtyCores > std::thread::hardware_concurrency())
    {
        std::cerr << "Cores quantity too high" << std::endl;
        return 1;
    }

    long long bufferSize = calculateBufferSize(percentLimit);

    try
    {
        buffer = new char[bufferSize];
        // para preencher o buffer rapido precisaria usar memset, mas nao da
        // para preencher com patterns
        fillBuffer(bufferSize, qtyCores);
    }
    catch (const std::bad_alloc& e)
    {
        std::cerr << "Memory allocation failed: " << e.what() << std::endl;
        return 1;
    }

    std::chrono::time_point finishTime = std::chrono::steady_clock::now() + std::chrono::minutes(minutesToRun);

    std::vector<std::thread> threads;


    // Aloca 2 threads por nucleo
    for (int i = 0; i < qtyCores * 2; i++)
    {
        // std::random_device randomDevice;
        // std::mt19937_64 threadTypeGenerator(randomDevice2());
        // std::uniform_int_distribution<long long> threadTypeDistribution(0, 2);
        if (i % 2 == 0)
        {
            threads.push_back(std::thread(invertBinaryValueThread, finishTime, bufferSize));
        } else {
            threads.push_back(std::thread(swapValuesThread, finishTime, bufferSize));
        }
    }

    // Impede que o programa feche antes das threads finalizarem
    for (auto& thread : threads) {
        thread.join();
    }

    delete[] buffer;

    return 0;
}
