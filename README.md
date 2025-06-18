# mem-stress
Programa de estressamento da memória principal, script construído em C++.

## Compilação
O programa depende apenas do cabeçalho da biblioteca CLI11, já inclusa no diretório `libs/`. Para compilar é necessário apenas um compilador, sendo alguns exemplos `g++`no Linux, compilador do Visual Studio, extensões do VSCode ou g++ através do MinGW no Windows.

Segue os exemplos:

- g++ (nativo ou pelo MinGW): `g++ mem-stress.cpp -o mem-stress`;
- Visual Studio: `cl /EHsc /std:c++17 mem-stress.cpp /Fe:mem-stress.exe`;
- VSCode: baixar extensão `Extension Pack for C/C++`

## Opções de execução
O programa oferece algumas opções para execução personalizada, estas são:

- `--help`: mostra as opções de execução;
- `--threads`: quatidade de threads que o programa vai rodar, impacta na sua velocidade e maior estresse da memória;
- `--perc`: porcentagem máximo de preenchimento da memória;
- `--min`: minutos de execução;

## Como funciona?
O programa funciona seguindo esses passos:

1) Procurar no sistema operacional a quantidade de memória virtual (RAM + Swap) livre e calcular o tamanho do buffer que vai ser preenchido de acordo com o que o usuário escolheu;
2) Preencher o buffer de memória, esse buffer é preenchido usando um padrão alternado de 0x55 e 0xAA;
3) Após preencher o buffer, vão ser invocadas uma série de threads que estressarão a memória fazendo operações repetidas nesse buffer, elas são:
    - Inverter os bits de uma posição aleatória
    - Trocar o valor entre duas posições aleatórias
4) Ao executar essas operações, o programa faz uma checagem se os valores foram atualizados corretamente, e caso salvarem algum valor errado, possivelmente há problema no hardware.

