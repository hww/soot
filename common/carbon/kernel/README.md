// example_usage.cpp
#include "VirtualMachine.hpp"
#include "BinaryFile.hpp"

int main() {
    using namespace vm;
    
    // Инициализация VM
    VirtualMachine& vm = VirtualMachine::get_instance();
    
    // Загрузка бинарного файла с байткодом
    vm.load_binary_file("game_code.bin");
    
    // Создание процессов с self-указателями (наш базис!)
    void* player_obj = /* создание игрового объекта */;
    void* enemy_obj = /* создание вражеского объекта */;
    
    Process* player_process = vm.create_process("player", player_obj);
    Process* enemy_process = vm.create_process("enemy", enemy_obj);
    
    // Настройка FSM для врага
    auto enemy_fsm = std::make_unique<StateMachine>(enemy_process);
    
    // Добавление состояний с байткодом
    FunctionDesc* patrol_code = vm.find_function("patrol-behavior");
    FunctionDesc* chase_code = vm.find_function("chase-behavior");
    
    if (patrol_code && chase_code) {
        auto patrol_state = std::make_unique<StateDesc>("patrol", patrol_code);
        auto chase_state = std::make_unique<StateDesc>("chase", chase_code);
        
        enemy_fsm->add_state(std::move(patrol_state));
        enemy_fsm->add_state(std::move(chase_state));
        enemy_fsm->set_initial_state("patrol");
        
        enemy_process->set_state_machine(std::move(enemy_fsm));
    }
    
    // Главный игровой цикл
    for (int frame = 0; frame < 1000; ++frame) {
        vm.execute_frame(); // Выполняем один кадр всех процессов
        
        // Другие игровые системы...
    }
    
    return 0;
}