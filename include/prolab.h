#ifndef PROLAB_H
#define PROLAB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#define PROLAB_TECHNICIAN_COUNT 4
#define PROLAB_TASK_TYPE_MAX 39
#define PROLAB_LOCATION_NAME_MAX 15
#define PROLAB_DEFAULT_TASK_CAPACITY 1000

typedef enum {
    TASK_NOT_ARRIVED,
    TASK_WAITING,
    TASK_RUNNING,
    TASK_COMPLETED
} TaskState;

typedef enum {
    ALGORITHM_NONE,
    ALGORITHM_SHORTEST_JOB,
    ALGORITHM_PRIORITY,
    ALGORITHM_ROUND_ROBIN,
    ALGORITHM_CUSTOM
} SchedulingAlgorithm;

typedef enum {
    TECHNICIAN_IDLE,
    TECHNICIAN_WORKING,
    TECHNICIAN_IN_TRANSIT
} TechnicianState;

typedef enum {
    LOCATION_LAB_A,
    LOCATION_LAB_B,
    LOCATION_CLASSROOM_1,
    LOCATION_CLASSROOM_2,
    LOCATION_CLASSROOM_3,
    LOCATION_COUNT
} Location;

typedef struct {
    int id;
    int arrival_time;
    int total_duration;
    int priority;
    char type[PROLAB_TASK_TYPE_MAX + 1];
    Location location;

    TaskState state;
    int remaining_duration;
    int worked_units;
    int current_technician; /* -1 when not assigned */
    int first_start_time;   /* -1 until first execution */
    int completion_time;    /* -1 until completion */
    int last_technician;    /* -1 until completed */
} Task;

typedef struct {
    TechnicianState state;
    int task_index;             /* -1 when no task is assigned */
    Location last_location;
    bool has_last_location;
    int transition_remaining;   /* separate from task and RR time */
    int worked_units;
    int quantum_remaining;      /* used only by Round Robin */
} Technician;

typedef struct {
    Task *tasks;
    size_t task_count;
    size_t task_capacity;

    int current_time;
    SchedulingAlgorithm algorithm;
    int round_robin_quantum;
    Technician technicians[PROLAB_TECHNICIAN_COUNT];

    /* The shared RR queue stores task indexes, not task IDs. */
    size_t *round_robin_queue;
    size_t round_robin_queue_head;
    size_t round_robin_queue_count;
    size_t round_robin_queue_capacity;
} Simulation;

typedef struct {
    size_t accepted_lines;
    size_t rejected_lines;
    size_t skipped_lines;
} TaskLoadReport;

/* Model lifecycle; task and queue storage is owned by Simulation. */
bool simulation_init(Simulation *simulation);
void simulation_destroy(Simulation *simulation);
void task_initialize(Task *task);

const char *task_state_name(TaskState state);
const char *algorithm_name(SchedulingAlgorithm algorithm);
const char *technician_state_name(TechnicianState state);
const char *location_name(Location location);

/* Loads valid records and reports malformed data lines; caller owns simulation. */
bool task_file_load(const char *path, Simulation *simulation,
                    TaskLoadReport *report, FILE *diagnostics);

/* Starts a fresh run. Round Robin requires a positive quantum; others use zero. */
bool simulation_start(Simulation *simulation, SchedulingAlgorithm algorithm,
                      int round_robin_quantum);
void simulation_reset(Simulation *simulation);
bool simulation_advance_by(Simulation *simulation, int units);
bool simulation_advance_to(Simulation *simulation, int target_time);

#endif
