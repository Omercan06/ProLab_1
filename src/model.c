#include "prolab.h"

#include <stdlib.h>
#include <string.h>

bool simulation_init(Simulation *simulation)
{
    size_t i;

    if (simulation == NULL) {
        return false;
    }

    memset(simulation, 0, sizeof(*simulation));
    simulation->task_capacity = PROLAB_DEFAULT_TASK_CAPACITY;
    simulation->tasks = calloc(simulation->task_capacity, sizeof(*simulation->tasks));
    if (simulation->tasks == NULL) {
        simulation->task_capacity = 0;
        return false;
    }

    simulation->algorithm = ALGORITHM_NONE;
    simulation->round_robin_quantum = 0;
    for (i = 0; i < PROLAB_TECHNICIAN_COUNT; ++i) {
        simulation->technicians[i].state = TECHNICIAN_IDLE;
        simulation->technicians[i].task_index = -1;
        simulation->technicians[i].transition_remaining = 0;
        simulation->technicians[i].quantum_remaining = 0;
    }

    return true;
}

void simulation_destroy(Simulation *simulation)
{
    if (simulation == NULL) {
        return;
    }

    free(simulation->tasks);
    free(simulation->round_robin_queue);
    memset(simulation, 0, sizeof(*simulation));
}

void task_initialize(Task *task)
{
    if (task == NULL) {
        return;
    }

    memset(task, 0, sizeof(*task));
    task->state = TASK_NOT_ARRIVED;
    task->current_technician = -1;
    task->first_start_time = -1;
    task->completion_time = -1;
    task->last_technician = -1;
}

const char *task_state_name(TaskState state)
{
    static const char *const names[] = {
        "Henüz gelmedi", "Bekliyor", "Devam ediyor", "Tamamlandı"
    };
    return state >= TASK_NOT_ARRIVED && state <= TASK_COMPLETED
               ? names[state]
               : "Bilinmiyor";
}

const char *algorithm_name(SchedulingAlgorithm algorithm)
{
    static const char *const names[] = {
        "Seçilmedi", "Shortest Job", "Priority", "Round Robin", "Özgün algoritma"
    };
    return algorithm >= ALGORITHM_NONE && algorithm <= ALGORITHM_CUSTOM
               ? names[algorithm]
               : "Bilinmiyor";
}

const char *technician_state_name(TechnicianState state)
{
    static const char *const names[] = { "Boş", "Meşgul", "Geçişte" };
    return state >= TECHNICIAN_IDLE && state <= TECHNICIAN_IN_TRANSIT
               ? names[state]
               : "Bilinmiyor";
}

const char *location_name(Location location)
{
    static const char *const names[] = {
        "LAB_A", "LAB_B", "DERSLIK_1", "DERSLIK_2", "DERSLIK_3"
    };
    return location >= LOCATION_LAB_A && location < LOCATION_COUNT
               ? names[location]
               : "Bilinmiyor";
}
