#include "prolab.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

static bool queue_reserve(Simulation *simulation, size_t required)
{
    size_t new_capacity;
    size_t i;
    size_t *new_queue;

    if (required <= simulation->round_robin_queue_capacity) {
        return true;
    }
    new_capacity = simulation->round_robin_queue_capacity == 0
                       ? PROLAB_DEFAULT_TASK_CAPACITY
                       : simulation->round_robin_queue_capacity;
    while (new_capacity < required) {
        if (new_capacity > (size_t)-1 / 2 / sizeof(*simulation->round_robin_queue)) {
            return false;
        }
        new_capacity *= 2;
    }

    new_queue = malloc(new_capacity * sizeof(*new_queue));
    if (new_queue == NULL) {
        return false;
    }
    for (i = 0; i < simulation->round_robin_queue_count; ++i) {
        new_queue[i] = simulation->round_robin_queue[
            (simulation->round_robin_queue_head + i) % simulation->round_robin_queue_capacity];
    }
    free(simulation->round_robin_queue);
    simulation->round_robin_queue = new_queue;
    simulation->round_robin_queue_capacity = new_capacity;
    simulation->round_robin_queue_head = 0;
    return true;
}

static bool queue_push(Simulation *simulation, size_t task_index)
{
    size_t tail;

    if (!queue_reserve(simulation, simulation->round_robin_queue_count + 1)) {
        return false;
    }
    tail = (simulation->round_robin_queue_head + simulation->round_robin_queue_count) %
           simulation->round_robin_queue_capacity;
    simulation->round_robin_queue[tail] = task_index;
    ++simulation->round_robin_queue_count;
    return true;
}

static bool queue_pop(Simulation *simulation, size_t *task_index)
{
    if (simulation->round_robin_queue_count == 0) {
        return false;
    }
    *task_index = simulation->round_robin_queue[simulation->round_robin_queue_head];
    simulation->round_robin_queue_head =
        (simulation->round_robin_queue_head + 1) % simulation->round_robin_queue_capacity;
    --simulation->round_robin_queue_count;
    return true;
}

static void reset_runtime_task(Task *task)
{
    task->state = TASK_NOT_ARRIVED;
    task->remaining_duration = task->total_duration;
    task->worked_units = 0;
    task->current_technician = -1;
    task->first_start_time = -1;
    task->completion_time = -1;
    task->last_technician = -1;
}

void simulation_reset(Simulation *simulation)
{
    size_t i;

    if (simulation == NULL) {
        return;
    }
    simulation->current_time = 0;
    simulation->round_robin_queue_head = 0;
    simulation->round_robin_queue_count = 0;
    for (i = 0; i < simulation->task_count; ++i) {
        reset_runtime_task(&simulation->tasks[i]);
    }
    for (i = 0; i < PROLAB_TECHNICIAN_COUNT; ++i) {
        Technician *technician = &simulation->technicians[i];
        technician->state = TECHNICIAN_IDLE;
        technician->task_index = -1;
        technician->last_location = LOCATION_LAB_A;
        technician->has_last_location = false;
        technician->transition_remaining = 0;
        technician->worked_units = 0;
        technician->quantum_remaining = 0;
    }
}

static bool needs_location_transition(const Technician *technician, const Task *task)
{
    return technician->has_last_location && technician->last_location != task->location;
}

static int compare_tasks(const Task *left, const Task *right,
                         SchedulingAlgorithm algorithm,
                         const Technician *technician)
{
    if (algorithm == ALGORITHM_CUSTOM) {
        long long left_cost = (long long)left->total_duration +
                              (needs_location_transition(technician, left) ? 1 : 0);
        long long right_cost = (long long)right->total_duration +
                               (needs_location_transition(technician, right) ? 1 : 0);
        bool left_transition = needs_location_transition(technician, left);
        bool right_transition = needs_location_transition(technician, right);

        if (left_cost != right_cost) {
            return left_cost < right_cost ? -1 : 1;
        }
        if (left_transition != right_transition) {
            return left_transition ? 1 : -1;
        }
        if (left->total_duration != right->total_duration) {
            return left->total_duration < right->total_duration ? -1 : 1;
        }
    }
    if (algorithm == ALGORITHM_SHORTEST_JOB && left->total_duration != right->total_duration) {
        return left->total_duration < right->total_duration ? -1 : 1;
    }
    if (algorithm == ALGORITHM_PRIORITY && left->priority != right->priority) {
        return left->priority < right->priority ? -1 : 1;
    }
    if (left->arrival_time != right->arrival_time) {
        return left->arrival_time < right->arrival_time ? -1 : 1;
    }
    if (left->id != right->id) {
        return left->id < right->id ? -1 : 1;
    }
    return 0;
}

static size_t choose_nonpreemptive_task(const Simulation *simulation,
                                        size_t technician_index)
{
    size_t selected = (size_t)-1;
    size_t i;
    const Technician *technician = &simulation->technicians[technician_index];

    for (i = 0; i < simulation->task_count; ++i) {
        if (simulation->tasks[i].state != TASK_WAITING) {
            continue;
        }
        if (selected == (size_t)-1 ||
            compare_tasks(&simulation->tasks[i], &simulation->tasks[selected],
                          simulation->algorithm, technician) < 0) {
            selected = i;
        }
    }
    return selected;
}

static bool process_arrivals(Simulation *simulation)
{
    size_t i;
    size_t accepted = 0;

    if (simulation->algorithm != ALGORITHM_ROUND_ROBIN) {
        for (i = 0; i < simulation->task_count; ++i) {
            Task *task = &simulation->tasks[i];
            if (task->state == TASK_NOT_ARRIVED && task->arrival_time <= simulation->current_time) {
                task->state = TASK_WAITING;
            }
        }
        return true;
    }

    /* Gather arrivals, then insert this timestamp's arrivals by ascending ID. */
    for (i = 0; i < simulation->task_count; ++i) {
        if (simulation->tasks[i].state == TASK_NOT_ARRIVED &&
            simulation->tasks[i].arrival_time <= simulation->current_time) {
            simulation->tasks[i].state = TASK_WAITING;
            ++accepted;
        }
    }
    if (accepted == 0) {
        return true;
    }
    {
        size_t *arrivals = malloc(accepted * sizeof(*arrivals));
        size_t count = 0;
        if (arrivals == NULL) {
            return false;
        }
        for (i = 0; i < simulation->task_count; ++i) {
            if (simulation->tasks[i].state == TASK_WAITING &&
                simulation->tasks[i].arrival_time == simulation->current_time) {
                size_t position = count;
                while (position > 0 && simulation->tasks[arrivals[position - 1]].id >
                                           simulation->tasks[i].id) {
                    arrivals[position] = arrivals[position - 1];
                    --position;
                }
                arrivals[position] = i;
                ++count;
            }
        }
        for (i = 0; i < count; ++i) {
            if (!queue_push(simulation, arrivals[i])) {
                free(arrivals);
                return false;
            }
        }
        free(arrivals);
    }
    return true;
}

static bool assign_task(Simulation *simulation, size_t technician_index,
                        size_t task_index)
{
    Technician *technician = &simulation->technicians[technician_index];
    Task *task = &simulation->tasks[task_index];

    task->state = TASK_RUNNING;
    task->current_technician = (int)technician_index;
    technician->task_index = (int)task_index;
    technician->quantum_remaining = simulation->algorithm == ALGORITHM_ROUND_ROBIN
                                        ? simulation->round_robin_quantum
                                        : 0;
    if (technician->has_last_location && technician->last_location != task->location) {
        technician->state = TECHNICIAN_IN_TRANSIT;
        technician->transition_remaining = 1;
    } else {
        technician->state = TECHNICIAN_WORKING;
        technician->transition_remaining = 0;
        if (task->first_start_time < 0) {
            task->first_start_time = simulation->current_time;
        }
    }
    technician->last_location = task->location;
    technician->has_last_location = true;
    return true;
}

static bool dispatch_tasks(Simulation *simulation)
{
    size_t technician_index;

    for (technician_index = 0; technician_index < PROLAB_TECHNICIAN_COUNT;
         ++technician_index) {
        Technician *technician = &simulation->technicians[technician_index];
        size_t task_index;
        bool found;

        if (technician->state != TECHNICIAN_IDLE) {
            continue;
        }
        if (simulation->algorithm == ALGORITHM_ROUND_ROBIN) {
            found = queue_pop(simulation, &task_index);
            while (found && simulation->tasks[task_index].state != TASK_WAITING) {
                found = queue_pop(simulation, &task_index);
            }
        } else {
            task_index = choose_nonpreemptive_task(simulation, technician_index);
            found = task_index != (size_t)-1;
        }
        if (found && !assign_task(simulation, technician_index, task_index)) {
            return false;
        }
    }
    return true;
}

static bool initialize_current_time(Simulation *simulation)
{
    return process_arrivals(simulation) && dispatch_tasks(simulation);
}

bool simulation_start(Simulation *simulation, SchedulingAlgorithm algorithm,
                      int round_robin_quantum)
{
    if (simulation == NULL || simulation->tasks == NULL ||
        algorithm < ALGORITHM_SHORTEST_JOB || algorithm > ALGORITHM_CUSTOM ||
        (algorithm == ALGORITHM_ROUND_ROBIN && round_robin_quantum <= 0) ||
        (algorithm != ALGORITHM_ROUND_ROBIN && round_robin_quantum != 0)) {
        return false;
    }
    simulation->algorithm = algorithm;
    simulation->round_robin_quantum = round_robin_quantum;
    simulation_reset(simulation);
    return initialize_current_time(simulation);
}

static bool advance_one_unit(Simulation *simulation)
{
    size_t i;
    size_t expired[PROLAB_TECHNICIAN_COUNT];
    size_t expired_count = 0;
    int next_time = simulation->current_time + 1;

    for (i = 0; i < PROLAB_TECHNICIAN_COUNT; ++i) {
        Technician *technician = &simulation->technicians[i];
        Task *task;

        if (technician->state == TECHNICIAN_IN_TRANSIT) {
            --technician->transition_remaining;
            if (technician->transition_remaining == 0) {
                technician->state = TECHNICIAN_WORKING;
                task = &simulation->tasks[technician->task_index];
                if (task->first_start_time < 0) {
                    task->first_start_time = next_time;
                }
            }
            continue;
        }
        if (technician->state != TECHNICIAN_WORKING) {
            continue;
        }

        task = &simulation->tasks[technician->task_index];
        --task->remaining_duration;
        ++task->worked_units;
        ++technician->worked_units;
        if (simulation->algorithm == ALGORITHM_ROUND_ROBIN) {
            --technician->quantum_remaining;
        }

        if (task->remaining_duration == 0) {
            task->state = TASK_COMPLETED;
            task->completion_time = next_time;
            task->current_technician = -1;
            task->last_technician = (int)i;
            technician->state = TECHNICIAN_IDLE;
            technician->task_index = -1;
            technician->transition_remaining = 0;
            technician->quantum_remaining = 0;
        } else if (simulation->algorithm == ALGORITHM_ROUND_ROBIN &&
                   technician->quantum_remaining == 0) {
            task->state = TASK_WAITING;
            task->current_technician = -1;
            technician->state = TECHNICIAN_IDLE;
            technician->task_index = -1;
            technician->transition_remaining = 0;
            technician->quantum_remaining = 0;
            expired[expired_count++] = (size_t)(task - simulation->tasks);
        }
    }

    simulation->current_time = next_time;
    if (!process_arrivals(simulation)) {
        return false;
    }
    /* The specification places new arrivals in the RR queue before expired slices. */
    for (i = 0; i < expired_count; ++i) {
        if (!queue_push(simulation, expired[i])) {
            return false;
        }
    }
    return dispatch_tasks(simulation);
}

bool simulation_advance_to(Simulation *simulation, int target_time)
{
    if (simulation == NULL || simulation->algorithm < ALGORITHM_SHORTEST_JOB ||
        simulation->algorithm > ALGORITHM_CUSTOM || target_time < 0) {
        return false;
    }
    if (target_time < simulation->current_time) {
        simulation_reset(simulation);
        if (!initialize_current_time(simulation)) {
            return false;
        }
    }
    while (simulation->current_time < target_time) {
        if (!advance_one_unit(simulation)) {
            return false;
        }
    }
    return true;
}

bool simulation_advance_by(Simulation *simulation, int units)
{
    if (simulation == NULL || units < 0 || units > INT_MAX - simulation->current_time) {
        return false;
    }
    return simulation_advance_to(simulation, simulation->current_time + units);
}
