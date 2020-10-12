#include <iostream>
#include <Windows.h>
#include <TlHelp32.h>
#include <math.h>
#include <random>
using namespace std;
constexpr auto PI = 3.14159265359;
DWORD PID = 0;
HANDLE pHandle;

struct Address_offsets
{
    DWORD _engine_module_address = 0; // cam
    DWORD _server_module_address = 0; // player
    DWORD CamY = 0x4791B4;
    DWORD CamX = 0x4791B8;
    DWORD NumOfPlayers = 0x3C0658; // engine.dll+3C0658
    DWORD IsCocked = 0xb38;
    DWORD local_base = 0x4F3FEC; // "server.dll"+4F3FEC; este é o 0, eu
    DWORD Life = 0x00E4;
    DWORD CoorY = 0x0288;
    DWORD CoorX = 0x0284;
    DWORD CoorZ = 0x0280;
    DWORD Team = 0x824;
}Address;

struct _FilledPlayers 
{
    DWORD Life;
    float CoorY;
    float CoorX;
    float CoorZ;
    float AimbotAngles[2] = {0};
    float D2DCam;
    DWORD Team;
    DWORD* baseplayer;
    float D3D;
    BOOL isCocked;
}FilledPlayers[32];

struct vec3 {
    float x, y, z;
};

vec3 Clamp(vec3 angle)
{
    //return angle;
    if (angle.x < -89.f) angle.x = -89.f;
    else if (angle.x > 89.f) angle.x = 89.f;
    return angle;
}

DWORD getPidByProcessName(char* processname) { // Retorna o PID do primeiro processo que encontramos com este nome
    PROCESSENTRY32 pe32; // cria um obj
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); // pega uma licença de acesso para snapshot de todos os processos
    Process32First(snapshot, &pe32); // escreve em um objeto o snapshot do 1° processo rodando na maquina

    // listar processos até que um deles seja o que estamos procurando
    bool isEnd = true;
    while (isEnd) {
        if (!strcmp(pe32.szExeFile, processname)) {
            CloseHandle(snapshot);
            return pe32.th32ProcessID;
        }
        isEnd = Process32Next(snapshot, &pe32); // retorna 0 se for o fim
    }
    CloseHandle(snapshot);
    return 0;
}

uintptr_t getModuleAddressByName(DWORD _PID, char* module_name) { // Retorna o ModuleAddress do primeiro modulo que encontramos com este nome
    //cout << "Module: " << module_name << endl;
    MODULEENTRY32 me32;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, _PID);
    Module32First(snapshot, &me32);
    bool isEnd = true;
    while (isEnd) {
        if (!strcmp(me32.szModule, module_name)) {
            CloseHandle(snapshot);
            return (uintptr_t)me32.modBaseAddr;
        }
        isEnd = Module32Next(snapshot, &me32);
    }
    CloseHandle(snapshot);
    return 0;
}

void setAimAngles(DWORD player) {
    float dy = FilledPlayers[0].CoorY - FilledPlayers[player].CoorY;
    float dx = FilledPlayers[0].CoorX - FilledPlayers[player].CoorX;
    float dz = FilledPlayers[0].CoorZ - FilledPlayers[player].CoorZ;
    int qr = 0, ac = 0; float co = 0, ca = 0;

    if (dx >= 0 && dz >= 0) { // 1° ok
        qr = 1; ac = 0; co = dx; ca = dz;
    }
    else if (dx <= 0 && dz >= 0) { // 2° ok
        qr = 2; ac = 3; co = dz; ca = dx * -1;
    }
    else if (dx <= 0 && dz <= 0) { // 3° ok
        qr = 3; ac = 2; co = dx * -1; ca = dz * -1;
    }
    else if (dx >= 0 && dz <= 0) { // 4° ok
        qr = 4; ac = 1; co = dz * -1; ca = dx;
    }

    float aim_angle_x = (atan2(co, ca) * (180 / (float)PI) + ac * 90) - 180; // -180 do jogo msm
    float aim_angle_y = (atan2(sqrt(pow(dx, 2) + pow(dz, 2)), dy) * (180 / (float)PI) - 90) * -1; // -90 do game msm
    float valorCameraXAtual = 0, valorCameraYAtual = 0;
    ReadProcessMemory(pHandle, (LPCVOID)(Address._engine_module_address + Address.CamX), &valorCameraXAtual, sizeof(DWORD), 0);
    ReadProcessMemory(pHandle, (LPCVOID)(Address._engine_module_address + Address.CamY), &valorCameraYAtual, sizeof(DWORD), 0);


    float dist_cam = sqrt(pow(valorCameraXAtual - aim_angle_x, 2) + pow(valorCameraYAtual - aim_angle_y, 2));


    vec3 diff, angle;
    diff.x = aim_angle_x - valorCameraXAtual;
    diff.y = aim_angle_y - valorCameraYAtual;
    diff.z = 0;
    angle.x = valorCameraXAtual;
    angle.y = valorCameraYAtual;

   // diff = Clamp(diff);

    angle.x += diff.x / ((rand() % 5) + 5); // suavidade e aleatoriedade, pra envitar detecção do ac   
    angle.y += diff.y / ((rand() % 5) + 5); // same
   // angle = Clamp(angle);

    FilledPlayers[player].AimbotAngles[0] = angle.x;
    FilledPlayers[player].AimbotAngles[1] = angle.y;
    FilledPlayers[player].D2DCam = dist_cam;
  //  cout << dist_cam_to_aim_x << endl;
    

}

void atualizaPonteirosPlayers(DWORD player) {
    ReadProcessMemory(pHandle, (LPCVOID)(Address._server_module_address + Address.local_base + (player * 0x10)), &FilledPlayers[player].baseplayer, sizeof(DWORD*), 0); // le a base de cada player
    ReadProcessMemory(pHandle, (LPCVOID)((DWORD)FilledPlayers[player].baseplayer + Address.Life), &FilledPlayers[player].Life,   sizeof(DWORD), 0); 
    ReadProcessMemory(pHandle, (LPCVOID)((DWORD)FilledPlayers[player].baseplayer + Address.CoorX), &FilledPlayers[player].CoorX, sizeof(float), 0);
    ReadProcessMemory(pHandle, (LPCVOID)((DWORD)FilledPlayers[player].baseplayer + Address.CoorY), &FilledPlayers[player].CoorY, sizeof(float), 0);
    ReadProcessMemory(pHandle, (LPCVOID)((DWORD)FilledPlayers[player].baseplayer + Address.CoorZ), &FilledPlayers[player].CoorZ, sizeof(float), 0);
    ReadProcessMemory(pHandle, (LPCVOID)((DWORD)FilledPlayers[player].baseplayer + Address.Team), &FilledPlayers[player].Team,   sizeof(DWORD), 0); 
    ReadProcessMemory(pHandle, (LPCVOID)((DWORD)FilledPlayers[player].baseplayer + Address.IsCocked), &FilledPlayers[player].isCocked, sizeof(DWORD), 0);
    FilledPlayers[player].D3D = (
        pow((FilledPlayers[0].CoorX - FilledPlayers[player].CoorX), 2) +
        pow((FilledPlayers[0].CoorY - FilledPlayers[player].CoorY), 2) +
        pow((FilledPlayers[0].CoorZ - FilledPlayers[player].CoorZ), 2));
    if (FilledPlayers[player].isCocked) {
        FilledPlayers[player].CoorY -= 18;
    }
    FilledPlayers[player].CoorY -= 10;

    if(FilledPlayers[player].Team != FilledPlayers[0].Team) // se n for meu time
        setAimAngles(player); // seta AimbotAngles e d2dcam
}





int main()
{
    // abre process
    PID = getPidByProcessName((char*)"hl2.exe"); // 32bits
    cout << "PROCESSO ENCONTRADO! PID = " << PID << endl;
    if (!PID) {
        cout << "Jogo não está aberto!" << endl;
        return 0;
    }
    pHandle = OpenProcess(PROCESS_ALL_ACCESS, false, PID);

    // pegar endereço dos modulos
    Address._engine_module_address = getModuleAddressByName(PID, (char*)"engine.dll");
    Address._server_module_address = getModuleAddressByName(PID, (char*)"server.dll");
    if(!Address._server_module_address || !Address._engine_module_address) {
        cout << "Jogo não está carregado!" << endl;
        return 0;
    }


    int numero_players_total = 0;
   
    while (true) {
        Sleep(1);
        ReadProcessMemory(pHandle, (LPCVOID)(Address._engine_module_address + Address.NumOfPlayers), &numero_players_total, sizeof(DWORD), 0);
        int player_mira_mais_proxima = 0;
        float menor_distancia_mira = 999999;

        for (int player = 0; player <= numero_players_total; player++) { // Poe o valor nas variaveis dos players
            atualizaPonteirosPlayers(player);
            if (FilledPlayers[player].D2DCam < menor_distancia_mira && player && FilledPlayers[player].Life > 1 && FilledPlayers[player].Team != FilledPlayers[0].Team) { // achar o player mais perto, vivo  e inimigo
                player_mira_mais_proxima = player;
                menor_distancia_mira = FilledPlayers[player].D2DCam;

            }   

        }
       // system("cls");
        //cout << FilledPlayers[player_mira_mais_proxima].D2DCam << endl;
        if (player_mira_mais_proxima > 0) { // se tiver alguem pra mirar
            if (GetAsyncKeyState(0x12)) { // ALT
                WriteProcessMemory(pHandle, (LPVOID)(Address._engine_module_address + Address.CamX), &FilledPlayers[player_mira_mais_proxima].AimbotAngles[0] , sizeof(float), 0);
                WriteProcessMemory(pHandle, (LPVOID)(Address._engine_module_address + Address.CamY), &FilledPlayers[player_mira_mais_proxima].AimbotAngles[1] , sizeof(float), 0);
            }
        }
    }
    return 0;
}
