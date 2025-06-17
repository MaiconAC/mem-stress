#include <iostream>
#include <cstring>
#include <random>
#include <thread>
#include <chrono>
#include <mutex>
#include <vector>
#include <string>
#include "libs/CLI11.hpp"
#include "sys/sysinfo.h"

#ifdef _WIN32
    #include <windows.h>
#endif

// Buffer que vai alocar a memoria do programa, volatile para evitar que o compilador otimize a leitura/escrita
volatile char * buffer = nullptr;
std::mutex bufferMutex;

// Contador de erros nas operacoes de memoria
int errCounter = 0;

long long getTotalAvailableVirtualMemory()
{

    #ifdef linux
        // cria uma variavel do tipo sysinfo
        struct sysinfo memInfo;

        // popula a veriavel com os dados do sistema
        sysinfo(&memInfo);

        // inicializa com o total de memoria disponivel
        long long totalMem = memInfo.freeram + memInfo.freeswap;
        totalMem *= memInfo.mem_unit; // converte para bytes

        return totalMem;
    #endif

    #ifdef _WIN32
        MEMORYSTATUSEX status;
        status.dwLength = sizeof(status);
        GlobalMemoryStatusEx(&status);
        return status.ullAvailPageFile;
    #endif
}

long long calculateBufferSize(int percentLimit)
{
    long long totalAvailablePhysicalMem = getTotalAvailableVirtualMemory();
    return (totalAvailablePhysicalMem * percentLimit) / 100;
}

// Thread que inverte o valor binario da posicao
void invertBinaryValueThread(std::chrono::time_point<std::chrono::steady_clock> finishTime, long long bufferSize)
{
    std::random_device randomDevice;
    std::mt19937_64 memPositionGenerator(randomDevice());
    std::uniform_int_distribution<long long> memPositionDistribution(0, bufferSize - 1);

    while (finishTime > std::chrono::steady_clock::now())
    {
        std::lock_guard<std::mutex> guard(bufferMutex);

        long long memoryPosition = memPositionDistribution(memPositionGenerator);

        if (memoryPosition >= bufferSize) continue;

        char oldData = buffer[memoryPosition];

        // Operador ~ inverte o valor binario
        buffer[memoryPosition] = ~buffer[memoryPosition];

        std::cout
            << "Posição " << memoryPosition << " inverteu os bits.\r"
            << std::flush;

        if (buffer[memoryPosition] != ~oldData)
        {
            errCounter++;
        }
    }
}

// Thread que faz o swap do valor de duas posicoes
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

        if (firstMemoryPosition >= bufferSize || secondMemoryPosition >= bufferSize) continue;

        char firstDataInMemory = buffer[firstMemoryPosition];
        char secondDataInMemory = buffer[secondMemoryPosition];

        // Operador ~ inverte o valor binario
        buffer[firstMemoryPosition] = secondDataInMemory;
        buffer[secondMemoryPosition] = firstDataInMemory;

        std::cout
            << "Posição " << firstMemoryPosition
            << " trocou de valor com " << secondMemoryPosition << ".\r"
            << std::flush;

        if (buffer[firstMemoryPosition] != secondDataInMemory || buffer[secondMemoryPosition] != firstDataInMemory)
        {
            errCounter++;
        }
    }
}

// Preenche parte do buffer com o padrao, nao usa mutex pois a posicao eh fixa
void writePattern(long long startIndex, long long finalIndex)
{
    for (long long i = startIndex; i < finalIndex; i++)
    {
        buffer[i] = i % 2 == 0 ? 0x55 : 0xAA;
    }
}

// Chamada para preecher buffer
void fillBuffer(long long bufferSize, int qtyThreads)
{
    std::vector<std::thread> threads;

    long long sizeToFill = bufferSize / (qtyThreads * 2);

    for (int i = 0; i < qtyThreads * 2; i++)
    {
        long long startIndex = i * sizeToFill;

        // Se for a ultima thread, preenche ate o final
        long long finalIndex = i == (qtyThreads * 2 - 1)
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
    // inicializa o CLI11, lib para passar parametros no executavel
    CLI::App app;

    int qtyThreads{4};
    app.add_option("--threads", qtyThreads, "Quantidade de threads para estressar a memória");

    int percentLimit{60};
    app.add_option("--perc", percentLimit, "Percentage limit of memory use");

    int minutesToRun{1};
    app.add_option("--min", minutesToRun, "Minutes to run");

    CLI11_PARSE(app, argc, argv);

    std::cout << "Inicializando estressador de memória!" << std::endl;
    std::cout << "Threads rodando: " << qtyThreads << std::endl;
    std::cout << "Limite de uso de memória (%): " << percentLimit << std::endl;
    std::cout << "Tempo para executar (min): " << minutesToRun << "\n" << std::endl;

    long long bufferSize = calculateBufferSize(percentLimit);

    try
    {
        std::cout << "Preenchendo o buffer de memória... " << std::flush;

        buffer = new char[bufferSize];
        fillBuffer(bufferSize, qtyThreads);

        std::cout << "Memória preenchida!\n" << std::endl;
    }
    catch (const std::bad_alloc& e)
    {
        std::cerr << "Memory allocation failed: " << e.what() << std::endl;
        return 1;
    }

    std::chrono::time_point finishTime = std::chrono::steady_clock::now() + std::chrono::minutes(minutesToRun);

    std::vector<std::thread> threads;

    // Aloca 2 threads por nucleo
    for (int i = 0; i < qtyThreads * 2; i++)
    {
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

    std::cout << std::endl;
    std::cout << "Quantidade detectada de erros de memória: " << errCounter << std::endl;
    std::cout << "Programa finalizado" << std::endl;

    return 0;
}
